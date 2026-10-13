Third-Party Libraries
This project uses the following third-party libraries:

memAllocator
Copyright (c) 2025-2026 IMSDcrueoft
License: MIT
Repository: https://github.com/IMSDcrueoft (authored in-repo)
License Location: third-party/memAllocator/LICENSE
Notice: This project includes memAllocator (`palloc_os`, `bits`, `arena_slab`), a thin OS virtual-memory primitive layer (mmap/mprotect/madvise on POSIX, VirtualAlloc/VirtualFree on Windows) topped by a one-tier small-object slab arena (≤256B size classes) used to speed up fixed-size object allocation in the VM. The memAllocator library is distributed under the MIT License. Please refer to the memAllocator repository for the full license text.
