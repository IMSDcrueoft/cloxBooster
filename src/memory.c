/*
* MIT License
* Copyright (c) 2025 IMSDcrueoft (https://github.com/IMSDcrueoft)
* See LICENSE file in the root directory for full license text.
*/
#include "memory.h"
#include "object.h"
#include "vm.h"
#include "allocator.h"
#include "gc.h"

// ---- slab layer ------------------------------------------------------------
// Fixed-size GC objects are served by the bundled arena slab (<= 256B size
// classes carved from one reserved segment). The segment is reserved once by
// slab_init() when the vm starts and released by slab_shutdown() when
// freeObjects() tears the vm down.

#if INTPTR_MAX >= INT64_MAX
#define SLAB_SEGMENT_EXPONENT SEGMENT_SIZE_EXPONENT_DEFAULT//4GB on 64-bit
#else
#define SLAB_SEGMENT_EXPONENT 28// fits 32-bit address spaces
#endif

bool slab_init()
{
	return arenaSlab_init(&arenaSlabDefault, SLAB_SEGMENT_EXPONENT);
}

void slab_trim()
{
	arenaSlab_trim(&arenaSlabDefault);
}

void slab_log_info() {
	arenaSlab_dumpStats(&arenaSlabDefault);
}

void slab_shutdown()
{
	arenaSlab_shutdown(&arenaSlabDefault);
}

void* slab_allocObject(size_t size)
{
	void* result = arenaSlab_alloc(&arenaSlabDefault, size);

#if LOG_EACH_MALLOC_INFO
	printf("[slab] alloc %p, %zu\n", result, size);
#endif

	if (result == NULL) {
		fprintf(stderr, "Slab allocation failed!\n");
		exit(1);
	}

	//count the live object into the gc bookkeeping
	vm.bytesAllocated += size;

	if (vm.bytesAllocated > vm.nextGC) {
		//the new object is not linked into vm.objects yet, so it's
		//invisible to the collector and can't be swept away here
		garbageCollect();
	}

	return result;
}

void slab_freeObject(size_t size, void* pointer)
{
	//discount it from the live bytes (symmetric with slab_allocObject)
	vm.bytesAllocated -= size;

#if LOG_EACH_MALLOC_INFO
	printf("[slab] free %p, %zu\n", pointer, size);
#endif

	arenaSlab_free(&arenaSlabDefault, pointer);
}

void* reallocate_no_gc(void* pointer, uint64_t oldSize, uint64_t newSize)
{
	vm.bytesAllocated_no_gc += newSize - oldSize;

	if (newSize == 0) {
		if (pointer != NULL) {
#if LOG_EACH_MALLOC_INFO
			printf("[mem] free %p\n", pointer);
#endif
			mem_free(pointer);
		}

		return NULL;
	}

	void* result = mem_realloc(pointer, newSize);

#if LOG_EACH_MALLOC_INFO
	printf("[mem] realloc %p -> %p, %zu\n", pointer, result, newSize);
#endif

	if (result == NULL) {
		fprintf(stderr, "Memory reallocation failed!\n");
		exit(1);
	}

	return result;
}

void* reallocate(void* pointer, uint64_t oldSize, uint64_t newSize)
{
	vm.bytesAllocated += newSize - oldSize;

	if (newSize > oldSize) {
#if DEBUG_STRESS_GC
		garbageCollect();
#endif
		if (vm.bytesAllocated > vm.nextGC) {
			garbageCollect();
		}
	}

	if (newSize == 0) {
		if (pointer != NULL) {
#if LOG_EACH_MALLOC_INFO
			printf("[mem] free %p\n", pointer);
#endif
			mem_free(pointer);
		}

		return NULL;
	}

	void* result = mem_realloc(pointer, newSize);

#if LOG_EACH_MALLOC_INFO
	printf("[mem] realloc %p -> %p, %zu\n", pointer, result, newSize);
#endif

	if (result == NULL) {
		fprintf(stderr, "Memory reallocation failed!\n");
		exit(1);
	}

	return result;
}

void freeObject(Obj* object) {
#if DEBUG_LOG_GC
	printf("[gc] %p free (%s)\n", (void*)object, objTypeInfo[object->type]);
#endif

	switch (object->type) {
	case OBJ_CLASS: {
		ObjClass* klass = (ObjClass*)object;
		table_free(&klass->methods);
		slab_freeObject(sizeof(ObjClass), object);
		break;
	}
	case OBJ_INSTANCE: {
		ObjInstance* instance = (ObjInstance*)object;
		table_free(&instance->fields);
		slab_freeObject(sizeof(ObjInstance), object);
		break;
	}
	case OBJ_CLOSURE: {
		ObjClosure* closure = (ObjClosure*)object;
		FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);

		slab_freeObject(sizeof(ObjClosure), object);
		break;
	}
	case OBJ_BOUND_METHOD: {
		slab_freeObject(sizeof(ObjBoundMethod), object);
		break;
	}
	case OBJ_UPVALUE:
		slab_freeObject(sizeof(ObjUpvalue), object);
		break;
	case OBJ_FUNCTION: {
		ObjFunction* function = (ObjFunction*)object;
		chunk_free(&function->chunk);
		FREE_NO_GC(ObjFunction, object);
		break;
	}
	case OBJ_NATIVE:
		FREE_NO_GC(ObjNative, object);
		break;
	case OBJ_STRING: {
		ObjString* string = (ObjString*)object;
		FREE_FLEX_NO_GC(ObjString, string, char, string->length + 1);//FAM object include'\0
		break;
	}
	}
}

void freeObjects()
{
#if DEBUG_LOG_GC
	printf("-- free dynamic objects\n");
#endif
	Obj* object = vm.objects;
	while (object != NULL) {
		Obj* next = OBJ_PTR_GET_NEXT(object);
		freeObject(object);
		object = next;
	}

	if (vm.grayStack != NULL) {
		mem_free(vm.grayStack);
	}

#if DEBUG_LOG_GC
	printf("-- free static objects\n");
#endif
	Obj* object_no_gc = vm.objects_no_gc;
	while (object_no_gc != NULL) {
		Obj* next = OBJ_PTR_GET_NEXT(object_no_gc);
		freeObject(object_no_gc);
		object_no_gc = next;
	}
}

void log_malloc_info()
{
	mem_print_stats(NULL);
}