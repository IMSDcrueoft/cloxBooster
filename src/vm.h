/*
 * MIT License
 * Copyright (c) 2025 IMSDcrueoft (https://github.com/IMSDcrueoft)
 * See LICENSE file in the root directory for full license text.
*/
#pragma once
#include "chunk.h"
#include "compiler.h"
#include "table.h"
#include "object.h"
#include "arena_bump.h"

//the depth of call frames
#define FRAMES_MAX 1024
//#define STACK_INITIAL_SIZE 4096 //unused since the arena stack: initial footprint is one 64KB commit chunk (= the old 4096 Values)
#define STACK_MAX_SIZE (FRAMES_MAX * UINT8_COUNT) //the max stack size, if exceed this, we can be sure it's a stack overflow
//vm stack arena: VA reserved once and NEVER moved, so growing the stack never
//invalidates frame->slots or upvalue->location pointers (no re-fix pass needed);
//physical memory is committed lazily in ARENA_BUMP_COMMIT_CHUNK steps
#define STACK_ARENA_SIZE (STACK_MAX_SIZE * sizeof(Value))

//vm string arena: same fixed-VA model as the stack arena; 512MiB reservation,
//committed lazily in 16KB steps (a single oversized string commits more at once)
#define STRING_ARENA_SIZE ((size_t)512 * 1024 * 1024)

typedef struct {
	ObjClosure* closure;
	uint8_t* ip;
	Value* slots; //first avilable slot
	//ptrdiff_t slotsOffset;// offset of the first slot in the vm stack, not a ptr, so that we can realloc the stack when needed
	//kept as a comment for future use: the stack now lives at a fixed arena base
	//(ArenaBumpAllocator) that never moves, so frame pointers never go stale and
	//no offset bookkeeping is needed; restore this field if the stack becomes movable again
} CallFrame;

typedef struct {
	//a cache
	Value* stack;
	Value* stackTop;
	//the only push boundary: the committed edge of the stack arena; crossing it
	//means commit more pages or (at the VA end = stackArena.capacity) overflow
	Value* stackBoundary;

	// deduplicated global constant table
	ValueArray constants;

	//scripts
	StringTable scripts;
	//pool
	StringTable strings;
	//pool
	NumberTable numbers;

	//upvalues
	ObjUpvalue* openUpvalues;

	//global hash table
	ObjInstance globals;

	//the root for dynamic objects
	Obj* objects;
	//the root for static objects
	Obj* objects_no_gc;

	//gc gray objects
	uint64_t grayCount;
	uint64_t grayCapacity;
	Obj** grayStack;

	//Excludes space used by stacks/constants/compilations
	uint64_t bytesAllocated_no_gc;
	uint64_t bytesAllocated;

	//Flip tagging, is more suitable for concurrent tagging
	uint8_t gcMark;
	//mark if the gc is running
	uint8_t gcWorking;
	//pad
	uint8_t padding[6];

	uint64_t beginGC;
	uint64_t nextGC;

	//literal object
	ObjClass emptyClass;

	ObjString* initString;

	//id for compiled functions
	uint32_t functionID;

	//frames
	uint32_t frameCount;
	CallFrame frames[FRAMES_MAX];

	//fixed-base backing storage for the stack (VA never moves, lazy commit)
	ArenaBumpAllocator stackArena;

	//fixed-base backing storage for interned strings (append-only bump): all
	//ObjString allocations come from here and the whole arena is released at
	//vm exit; strings are NOT linked into the object lists anymore
	ArenaBumpAllocator stringArena;
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

//the global shared vm extern to other file
extern VM vm;

void vm_init();
void vm_free();

void stack_push(Value value);
Value stack_pop();
void stack_replace(Value val);

//get the size of constants (including the holes)
uint32_t getConstantSize();
//add a constant and get the index of it,i will handle it later
uint32_t addConstant(Value value);

//bytes of the vm stack arena currently committed (physical memory in use)
uint64_t stackCommittedBytes();
//bytes of the vm string arena currently committed (physical memory in use)
uint64_t stringCommittedBytes();

InterpretResult interpret(C_STR source);
InterpretResult interpret_repl(C_STR source);

//for global
void defineNative_global(C_STR name, NativeFn function);