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
// One cache per slab-served object type. Caches are constructed lazily on the
// first allocation of that type and all of them are destroyed in
// slab_freeCaches() when freeObjects() tears the vm down.

#define SLAB_RESERVED_LIMIT 4

static SlabAllocator* slabCaches[OBJ_TOTAL_COUNT] = { 0 };

//upper allocator callbacks (the standard library allocator)
static void* slab_upper_malloc(void* unused, size_t size) {
	(void)unused;
	return mem_alloc(size);
}

static void slab_upper_free(void* unused, void* pointer) {
	(void)unused;
	if (pointer != NULL) {
		mem_free(pointer);
	}
}

void* slab_allocObject(unsigned int objType, size_t size)
{
	SlabAllocator* cache = slabCaches[objType];
	if (cache == NULL) {
		//the unit payload must hold the whole object
		cache = slab_createAllocator(
			(uint32_t)size,
			SLAB_RESERVED_LIMIT,
			NULL, slab_upper_malloc, slab_upper_free);

		if (cache == NULL) {
			fprintf(stderr, "Slab allocator creation failed!\n");
			exit(1);
		}

		slabCaches[objType] = cache;
	}

	void* result = slab_allocate(cache);

#if LOG_EACH_MALLOC_INFO
	printf("[slab] alloc %p, %zu for (%u)\n", result, size, objType);
#endif

	if (result == NULL) {
		fprintf(stderr, "Slab allocation failed!\n");
		exit(1);
	}

	//count the live unit into the gc bookkeeping (the free ones in the cache don't)
	vm.bytesAllocated += size;

	if (vm.bytesAllocated > vm.nextGC) {
		//the new object is not linked into vm.objects yet, so it's
		//invisible to the collector and can't be swept away here
		garbageCollect();
	}

	return result;
}

void slab_freeObject(unsigned int objType, void* pointer)
{
	SlabAllocator* cache = slabCaches[objType];

	//the unit is returned to the cache, discount it from the live bytes
	vm.bytesAllocated -= slab_unitSize(cache);

#if LOG_EACH_MALLOC_INFO
	printf("[slab] free %p (%u)\n", pointer, objType);
#endif

	//objects are always allocated after the cache exists
	slab_deallocate(cache, pointer);
}

void slab_freeCaches()
{
	for (unsigned int type = 0; type < OBJ_TOTAL_COUNT; type++) {
		if (slabCaches[type] != NULL) {
			slab_destroyAllocator(slabCaches[type]);
			slabCaches[type] = NULL;
		}
	}
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
		FREE(ObjClass, object);
		break;
	}
	case OBJ_INSTANCE: {
		ObjInstance* instance = (ObjInstance*)object;
		table_free(&instance->fields);
		slab_freeObject(OBJ_INSTANCE, object);
		break;
	}
	case OBJ_CLOSURE: {
		ObjClosure* closure = (ObjClosure*)object;
		FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);

		slab_freeObject(OBJ_CLOSURE, object);
		break;
	}
	case OBJ_BOUND_METHOD: {
		slab_freeObject(OBJ_BOUND_METHOD, object);
		break;
	}
	case OBJ_UPVALUE:
		slab_freeObject(OBJ_UPVALUE, object);
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

	//all slab objects are gone, release the caches
	slab_freeCaches();
}

void log_malloc_info()
{
	mem_print_stats(NULL);
}