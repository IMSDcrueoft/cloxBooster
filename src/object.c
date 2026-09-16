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

//slab-served objects: fixed size, gc-able, allocated from per-type caches
#define ALLOCATE_OBJ_SLAB(type, objectType) \
    (type*)allocateObject_slab(sizeof(type), objectType)

HOT_FUNCTION
static Obj* allocateObject_slab(uint64_t size, ObjType type) {
	Obj* object = (Obj*)slab_allocObject(type, size);

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
	case OBJ_FUNCTION:
	case OBJ_NATIVE:
	case OBJ_STRING:
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
ObjUpvalue* newUpvalue(Value* slot, ptrdiff_t offset)
{
	ObjUpvalue* upvalue = ALLOCATE_OBJ_SLAB(ObjUpvalue, OBJ_UPVALUE);
	upvalue->location = slot;
	upvalue->location_offset = offset;
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
	ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
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

//will check '\\' '\"'
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

			//stack_push(OBJ_VAL(string));
			tableSet_string(&vm.strings, string);
			//stack_pop();
		}

		return string;
	}
	else {
		uint32_t actualLength = 0;

		for (uint32_t i = 0; i < length; ++i) {
			if (chars[i] == '\\') {
				if (i + 1 < length) {
					switch (chars[i + 1]) {
					case '\\':
					case '\"':
					case 'n':
						++i;
						break;
					default:
						++actualLength;
						++i;
						break;
					}
				}
			}

			++actualLength;
		}

		heapSize += actualLength;
		string = ALLOCATE_FLEX_OBJ(ObjString, OBJ_STRING, heapSize);

		for (uint32_t readIndex = 0, writeIndex = 0; readIndex < length;) {
			uint32_t start = readIndex;

			while (readIndex < length && chars[readIndex] != '\\') {
				++readIndex;
			}

			if (start < readIndex) {
				memcpy(string->chars + writeIndex, chars + start, readIndex - start);
				writeIndex += readIndex - start;
			}

			if ((readIndex + 1) < length) {
				switch (chars[readIndex + 1]) {
				case '\\':
				case '\"':
					string->chars[writeIndex] = chars[readIndex + 1];
					++writeIndex;
					break;
				case 'n':
					string->chars[writeIndex] = '\n';
					++writeIndex;
					break;
				default:
					string->chars[writeIndex] = '\\';
					++writeIndex;
					string->chars[writeIndex] = chars[readIndex + 1];
					++writeIndex;
					break;
				}
				readIndex += 2;
			}
		}

		string->chars[actualLength] = '\0';
		string->length = actualLength;

		//the string escaped
		hash = HASH_64bits(string->chars, string->length);
		string->hash = hash;
		string->symbol = INVALID_OBJ_STRING_SYMBOL;

		//find deduplicate one
		ObjString* interned = deduplicateString(string->chars, string->length, string->hash);
		if (interned == NULL) {
			//stack_push(OBJ_VAL(string));
			tableSet_string(&vm.strings, string);
			//stack_pop();
			return string;
		}
		else {
			//free memory this should be the first obj of the object list
			vm.objects_no_gc = OBJ_PTR_GET_NEXT(&string->obj);
			freeObject((Obj*)string);
			return interned;
		}
	}
}

ObjString* connectString(ObjString* strA, ObjString* strB) {
	uint32_t heapSize = sizeof(ObjString) + strA->length + strB->length + 1;
	ObjString* string = ALLOCATE_FLEX_OBJ(ObjString, OBJ_STRING, heapSize);

	memcpy(string->chars, strA->chars, strA->length);
	memcpy(string->chars + strA->length, strB->chars, strB->length);
	string->chars[strA->length + strB->length] = '\0';
	string->length = strA->length + strB->length;

	uint64_t hash = HASH_64bits(string->chars, string->length);
	string->hash = hash;
	string->symbol = INVALID_OBJ_STRING_SYMBOL;

	//do deduplicate
	ObjString* interned = deduplicateString(string->chars, string->length, string->hash);
	if (interned == NULL) {
		//stack_push(OBJ_VAL(string));
		tableSet_string(&vm.strings, string);
		//stack_pop();
		return string;
	}
	else {
		//free memory
		vm.objects_no_gc = OBJ_PTR_GET_NEXT(&string->obj);
		freeObject((Obj*)string);
		return interned;
	}
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