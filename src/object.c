/*
 * MIT License
 * Copyright (c) 2025 IMSDcrueoft (https://github.com/IMSDcrueoft)
 * See LICENSE file in the root directory for full license text.
*/
#include "object.h"
#include "vm.h"
#include "hash.h"
#include "memory.h"
#include "gc.h"
#include "allocator.h"

#if DEBUG_LOG_GC
const C_STR objTypeInfo[] = {
	[OBJ_CLASS] = {"class"},
	[OBJ_CLOSURE] = {"closure"},
	[OBJ_FUNCTION] = {"function"},
	[OBJ_NATIVE] = {"native"},
	[OBJ_UPVALUE] = {"upValue"},
	[OBJ_STRING] = {"string"},
};
#endif

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

#define ALLOCATE_FLEX_OBJ(type,objectType,byteSize) \
    (type*)allocateObject(byteSize, objectType)

//slab-served objects: fixed size, gc-able, allocated from the arena slab
#define ALLOCATE_OBJ_SLAB(type, objectType) \
    (type*)allocateObject_slab(sizeof(type), objectType)

HOT_FUNCTION
static Obj* allocateObject_slab(uint64_t size, ObjType type) {
	Obj* object = (Obj*)slab_allocObject(size);

	//link the objects
	OBJ_PTR_SET_NEXT(object, vm.objects);
	object->type = type;
	object->isMarked = !vm.gcMark;
	vm.objects = object;

#if DEBUG_LOG_GC
	printf("[gc] %p allocate %zu for (%s)\n", (void*)object, size, objTypeInfo[type]);
#endif

	return object;
}

HOT_FUNCTION
static Obj* allocateObject(uint64_t size, ObjType type) {
	Obj* object = NULL;

	//link the objects
	switch (type) {
	case OBJ_STRING:
	{
		//strings come from the fixed-VA string arena (append-only bump, the
		//whole arena is released at vm exit); they are NOT linked into the
		//object lists anymore, the arena owns their lifetime
		ObjString* string = (ObjString*)arenaBump_alloc(&vm.stringArena, (size_t)size);
		if (string == NULL) {
			fprintf(stderr, "Memory reallocation failed!\n");
			exit(1);
		}

		memset(&string->obj, 0, sizeof(Obj));
		string->obj.type = OBJ_STRING;
		object = (Obj*)string;
		break;
	}
	case OBJ_FUNCTION:
	case OBJ_NATIVE:
		object = (Obj*)reallocate_no_gc(NULL, 0, size);
		OBJ_PTR_SET_NEXT(object, vm.objects_no_gc);
		object->type = type;
		object->isMarked = !vm.gcMark;
		vm.objects_no_gc = object;
		break;
	default:
		object = (Obj*)reallocate(NULL, 0, size);
		OBJ_PTR_SET_NEXT(object, vm.objects);
		object->type = type;
		object->isMarked = !vm.gcMark;
		vm.objects = object;
		break;
	}

#if DEBUG_LOG_GC
	printf("[gc] %p allocate %zu for (%s)\n", (void*)object, size, objTypeInfo[type]);
#endif

	return object;
}

HOT_FUNCTION
ObjUpvalue* newUpvalue(Value* slot)
{
	ObjUpvalue* upvalue = ALLOCATE_OBJ_SLAB(ObjUpvalue, OBJ_UPVALUE);
	upvalue->location = slot;
	//upvalue->location_offset = offset; //unused since the arena stack (fixed base, never re-fixed)
	upvalue->closed = NIL_VAL;
	upvalue->next = NULL;
	return upvalue;
}

ObjFunction* newFunction() {
	ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
	function->arity = 0;
	function->upvalueCount = 0;
	function->id = vm.functionID++;//unique id
	function->name = NULL;
	chunk_init(&function->chunk);
	return function;
}

HOT_FUNCTION
ObjClosure* newClosure(ObjFunction* function) {
	ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
	for (uint32_t i = 0; i < function->upvalueCount; i++) {
		upvalues[i] = NULL;
	}

	ObjClosure* closure = ALLOCATE_OBJ_SLAB(ObjClosure, OBJ_CLOSURE);
	closure->function = function;
	closure->upvalues = upvalues;
	closure->upvalueCount = function->upvalueCount;

	return closure;
}

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method)
{
	ObjBoundMethod* bound = ALLOCATE_OBJ_SLAB(ObjBoundMethod, OBJ_BOUND_METHOD);
	bound->receiver = receiver;
	bound->method = method;
	return bound;
}

//create native function
ObjNative* newNative(NativeFn function) {
	ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
	native->function = function;
	return native;
}

ObjClass* newClass(ObjString* name)
{
	ObjClass* klass = ALLOCATE_OBJ_SLAB(ObjClass, OBJ_CLASS);
	klass->name = name;
	klass->initializer = NIL_VAL;
	klass->methods.isGlobal = false;
	klass->methods.isFrozen = false;
	table_init(&klass->methods);
	return klass;
}

HOT_FUNCTION
ObjInstance* newInstance(ObjClass* klass) {
	ObjInstance* instance = ALLOCATE_OBJ_SLAB(ObjInstance, OBJ_INSTANCE);
	instance->klass = klass;
	instance->fields.isGlobal = false;
	instance->fields.isFrozen = false;
	table_init(&instance->fields);
	return instance;
}

//if find deduplicate one return it else null
static inline ObjString* deduplicateString(C_STR chars, uint32_t length, uint64_t hash) {
	return tableFindString(&vm.strings, chars, length, hash);
}

//will check '\\' '\"' '\n' '\t'
ObjString* copyString(C_STR chars, uint32_t length, bool escapeChars)
{
	uint32_t heapSize = sizeof(ObjString) + 1;
	ObjString* string = NULL;
	uint64_t hash;

	if (!escapeChars) {
		hash = HASH_64bits(chars, length);

		//find deduplicate one
		string = deduplicateString(chars, length, hash);
		if (string == NULL) {
			//create string
			heapSize += length;
			string = ALLOCATE_FLEX_OBJ(ObjString, OBJ_STRING, heapSize);

			memcpy(string->chars, chars, length);
			string->chars[length] = '\0';
			string->length = length;

			string->hash = hash;
			string->symbol = INVALID_OBJ_STRING_SYMBOL;

			tableSet_string(&vm.strings, string);
		}

		return string;
	}
	else {
		//the unescaped content is never longer than the source, so the source
		//length bounds the scratch need; short literals stay on the stack,
		//long ones use a transient heap buffer (always released below)
		char stackScratch[512];// no init
		char* scratch = NULL;
		char* content = stackScratch;
		if (length > (uint32_t)sizeof(stackScratch)) {
			scratch = ALLOCATE_NO_GC(char, length);
			content = scratch;
		}

		//single fused pass: unescape into the scratch (writeIndex ends up as
		//the unescaped length; unescaping never grows the content)
		uint32_t writeIndex = 0;
		for (uint32_t readIndex = 0; readIndex < length;) {
			//copy the run of plain characters
			uint32_t start = readIndex;
			while (readIndex < length && chars[readIndex] != '\\') {
				++readIndex;
			}
			if (start < readIndex) {
				memcpy(content + writeIndex, chars + start, readIndex - start);
				writeIndex += readIndex - start;
			}

			if (readIndex >= length) {
				break;
			}

			++readIndex;//consume the backslash
			if (readIndex < length) {
				switch (chars[readIndex]) {
				case '\\':
				case '\"':
					content[writeIndex++] = chars[readIndex];
					break;
				case 'n':
					content[writeIndex++] = '\n';
					break;
				case 't':
					content[writeIndex++] = '\t';
					break;
				default:
					//unknown escape: keep both characters verbatim
					content[writeIndex++] = '\\';
					content[writeIndex++] = chars[readIndex];
					break;
				}
				++readIndex;
			}
			else {
				//trailing backslash: keep it verbatim (unreachable via the
				//scanner, which treats \" as an escaped quote)
				content[writeIndex++] = '\\';
			}
		}

		//probe the pool with the unescaped content: a hit costs zero allocations
		//(bump strings cannot be freed one by one, so never allocate before the dedup)
		const uint64_t escapedHash = HASH_64bits(content, writeIndex);
		string = deduplicateString(content, writeIndex, escapedHash);
		if (string == NULL) {
			//genuinely new content: allocate, copy, intern
			string = ALLOCATE_FLEX_OBJ(ObjString, OBJ_STRING, sizeof(ObjString) + 1 + writeIndex);
			memcpy(string->chars, content, writeIndex);
			string->chars[writeIndex] = '\0';
			string->length = writeIndex;
			string->hash = escapedHash;
			string->symbol = INVALID_OBJ_STRING_SYMBOL;

			tableSet_string(&vm.strings, string);
		}

		if (scratch != NULL) {
			FREE_ARRAY_NO_GC(char, scratch, length);
		}

		return string;
	}
}

ObjString* connectString(ObjString* strA, ObjString* strB) {
	const uint32_t length = strA->length + strB->length;

	//hash the two segments without materializing the joined content
	HashStream stream;
	hashStreamBegin(&stream, length);
	hashStreamUpdate(&stream, strA->chars, strA->length);
	hashStreamUpdate(&stream, strB->chars, strB->length);
	const uint64_t hash = hashStreamFinalize(&stream);

	//probe first: a dedup hit costs zero allocations and no list surgery
	//(bump strings cannot be freed one by one)
	ObjString* interned = tableFindConcatString(&vm.strings, strA->chars, strA->length, strB->chars, strB->length, hash);
	if (interned != NULL) {
		return interned;
	}

	//genuinely new content: allocate, assemble, intern
	ObjString* string = ALLOCATE_FLEX_OBJ(ObjString, OBJ_STRING, sizeof(ObjString) + length + 1);

	memcpy(string->chars, strA->chars, strA->length);
	memcpy(string->chars + strA->length, strB->chars, strB->length);
	string->chars[length] = '\0';
	string->length = length;
	string->hash = hash;
	string->symbol = INVALID_OBJ_STRING_SYMBOL;

	tableSet_string(&vm.strings, string);
	return string;
}

//memcmp-style three-way ordering for string pairs:
//byte compare over the common prefix, then the shorter operand is the smaller one
int compareObjStrings(const ObjString* a, const ObjString* b) {
	uint32_t common = (a->length < b->length) ? a->length : b->length;
	int cmp = memcmp(a->chars, b->chars, common);
	if (cmp != 0) return cmp;
	return (a->length < b->length) ? -1 : ((a->length > b->length) ? 1 : 0);
}

static void printFunction(ObjFunction* function) {
	if (function->name == NULL) {
		printf("<script> (%d)", function->id);
	}
	else if (function->name->length == 0) {
		printf("<lambda> (%d)", function->id);
	}
	else {
		printf("<fn %s> (%d)", function->name->chars, function->id);
	}
}

void printObject(Value value, bool isExpand) {
	switch (OBJ_TYPE(value)) {
	case OBJ_CLASS: {
		ObjString* name = AS_CLASS(value)->name;
		if (name != NULL) {
			printf("%s (class)", name->chars);
		}
		else {
			printf("$anon (class)");
		}
		break;
	}
	case OBJ_INSTANCE: {
		ObjClass* klass = AS_INSTANCE(value)->klass;
		if (klass != NULL) {
			printf("%s (instance)", klass->name->chars);
		}
		else {
			printf("$anon (instance)");
		}
		break;
	}
	case OBJ_BOUND_METHOD:
		printFunction(AS_BOUND_METHOD(value)->method->function);
		break;
	case OBJ_CLOSURE:
		printFunction(AS_CLOSURE(value)->function);
		break;
	case OBJ_FUNCTION:
		printFunction(AS_FUNCTION(value));
		break;
	case OBJ_NATIVE:
		printf("<native fn>");
		break;
	case OBJ_STRING:
		printf("%s", AS_STRING(value)->chars);
		break;
	case OBJ_UPVALUE:
		printf("upvalue");
		break;
	}
}

StringEntry* getStringEntryInPool(ObjString* string)
{
	return tableGetStringEntry(&vm.strings, string);
}

NumberEntry* getNumberEntryInPool(Value* value)
{
	return tableGetNumberEntry(&vm.numbers, value);
}