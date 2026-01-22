//
// Copyright 2023 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/imaging/hgiVulkan/vk_mem_alloc.h"

#define VMA_IMPLEMENTATION

#if defined(ARCH_OS_ANDROID)
#define VMA_VULKAN_VERSION 1001000
#endif

#include <vma/vk_mem_alloc.h>
