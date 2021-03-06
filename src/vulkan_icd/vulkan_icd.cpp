/*
 * Copyright 2017 Jacob Lifshay
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include "vulkan_icd.h"
#include "util/string_view.h"
#include <initializer_list>
#include <iostream>
#include <atomic>
#include "wsi.h"
#include "pipeline/pipeline.h"

using namespace kazan;

#ifdef __ELF__
#define DLLEXPORT_ATTR(original_attributes) \
    __attribute__((visibility("default"))) original_attributes
#define DLLEXPORT_CALL(original_attributes) original_attributes
#elif defined(_WIN32)
#define DLLEXPORT_ATTR(original_attributes) __declspec(dllexport) original_attributes
#define DLLEXPORT_CALL(original_attributes) original_attributes
#else
#error DLLEXPORT_* macros not implemented for platform
#endif

extern "C" DLLEXPORT_ATTR(VKAPI_ATTR) PFN_vkVoidFunction DLLEXPORT_CALL(VKAPI_CALL)
    vk_icdGetInstanceProcAddr(VkInstance instance, const char *name)
{
    return vulkan_icd::Vulkan_loader_interface::get()->get_instance_proc_addr(instance, name);
}

static_assert(static_cast<PFN_vk_icdGetInstanceProcAddr>(&vk_icdGetInstanceProcAddr), "");

static constexpr void validate_allocator(const VkAllocationCallbacks *allocator) noexcept
{
    assert(allocator == nullptr && "Vulkan allocation callbacks are not implemented");
}

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance,
                                                                          const char *name)
{
    return vk_icdGetInstanceProcAddr(instance, name);
}

static_assert(static_cast<PFN_vkGetInstanceProcAddr>(&vkGetInstanceProcAddr), "");

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *create_info,
                                                           const VkAllocationCallbacks *allocator,
                                                           VkInstance *instance)
{
    return vulkan_icd::Vulkan_loader_interface::get()->create_instance(
        create_info, allocator, instance);
}

static_assert(static_cast<PFN_vkCreateInstance>(&vkCreateInstance), "");

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
    const char *layer_name, uint32_t *property_count, VkExtensionProperties *properties)

{
    return vulkan_icd::Vulkan_loader_interface::get()->enumerate_instance_extension_properties(
        layer_name, property_count, properties);
}

static_assert(static_cast<PFN_vkEnumerateInstanceExtensionProperties>(
                  &vkEnumerateInstanceExtensionProperties),
              "");

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance instance,
                                                        const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    vulkan::Vulkan_instance::move_from_handle(instance).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(
    VkInstance instance, uint32_t *physical_device_count, VkPhysicalDevice *physical_devices)
{
    assert(instance);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto *instance_pointer = vulkan::Vulkan_instance::from_handle(instance);
            return vulkan_icd::vulkan_enumerate_list_helper(
                physical_device_count,
                physical_devices,
                {
                    to_handle(&instance_pointer->physical_device),
                });
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice physical_device, VkPhysicalDeviceFeatures *features)
{
    assert(physical_device);
    assert(features);
    auto *physical_device_pointer = vulkan::Vulkan_physical_device::from_handle(physical_device);
    *features = physical_device_pointer->features;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice physical_device, VkFormat format, VkFormatProperties *format_properties)
{
    assert(physical_device);
    assert(format_properties);
    *format_properties = vulkan::get_format_properties(format);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice,
                                             VkFormat format,
                                             VkImageType type,
                                             VkImageTiling tiling,
                                             VkImageUsageFlags usage,
                                             VkImageCreateFlags flags,
                                             VkImageFormatProperties *pImageFormatProperties)
{
#warning finish implementing vkGetPhysicalDeviceImageFormatProperties
    assert(!"vkGetPhysicalDeviceImageFormatProperties is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(
    VkPhysicalDevice physical_device, VkPhysicalDeviceProperties *properties)
{
    assert(physical_device);
    assert(properties);
    auto *physical_device_pointer = vulkan::Vulkan_physical_device::from_handle(physical_device);
    *properties = physical_device_pointer->properties;
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physical_device,
                                             uint32_t *queue_family_property_count,
                                             VkQueueFamilyProperties *queue_family_properties)
{
    assert(physical_device);
    auto *physical_device_pointer = vulkan::Vulkan_physical_device::from_handle(physical_device);
    vulkan_icd::vulkan_enumerate_list_helper(
        queue_family_property_count,
        queue_family_properties,
        physical_device_pointer->queue_family_properties,
        vulkan::Vulkan_physical_device::queue_family_property_count);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physical_device, VkPhysicalDeviceMemoryProperties *memory_properties)
{
    assert(physical_device);
    assert(memory_properties);
    auto *physical_device_pointer = vulkan::Vulkan_physical_device::from_handle(physical_device);
    *memory_properties = physical_device_pointer->memory_properties;
}

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device,
                                                                        const char *name)
{
    assert(device);
    auto *device_pointer = vulkan::Vulkan_device::from_handle(device);
    return vulkan_icd::Vulkan_loader_interface::get()->get_procedure_address(
        name,
        vulkan_icd::Vulkan_loader_interface::Procedure_address_scope::Device,
        &device_pointer->instance,
        device_pointer);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physical_device,
                                                         const VkDeviceCreateInfo *create_info,
                                                         const VkAllocationCallbacks *allocator,
                                                         VkDevice *device)
{
    validate_allocator(allocator);
    assert(create_info);
    assert(physical_device);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_device::create(
                *vulkan::Vulkan_physical_device::from_handle(physical_device), *create_info);
            if(util::holds_alternative<VkResult>(create_result))
                return util::get<VkResult>(create_result);
            *device = move_to_handle(
                util::get<std::unique_ptr<vulkan::Vulkan_device>>(std::move(create_result)));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device,
                                                      const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    vulkan::Vulkan_device::move_from_handle(device).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physical_device,
                                         const char *layer_name,
                                         uint32_t *property_count,
                                         VkExtensionProperties *properties)
{
    return vulkan_icd::Vulkan_loader_interface::get()->enumerate_device_extension_properties(
        physical_device, layer_name, property_count, properties);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkEnumerateInstanceLayerProperties(uint32_t *pPropertyCount, VkLayerProperties *pProperties)
{
#warning finish implementing vkEnumerateInstanceLayerProperties
    assert(!"vkEnumerateInstanceLayerProperties is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(
    VkPhysicalDevice physicalDevice, uint32_t *pPropertyCount, VkLayerProperties *pProperties)
{
#warning finish implementing vkEnumerateDeviceLayerProperties
    assert(!"vkEnumerateDeviceLayerProperties is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(VkDevice device,
                                                       uint32_t queue_family_index,
                                                       uint32_t queue_index,
                                                       VkQueue *queue)
{
    assert(device);
    assert(queue_family_index < vulkan::Vulkan_physical_device::queue_family_property_count);
    assert(queue_index < vulkan::Vulkan_device::queue_count);
    assert(queue);
    auto *device_pointer = vulkan::Vulkan_device::from_handle(device);
    static_assert(vulkan::Vulkan_device::queue_count == 1, "");
    *queue = to_handle(device_pointer->queues[0].get());
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue,
                                                        uint32_t submit_count,
                                                        const VkSubmitInfo *submits,
                                                        VkFence fence)
{
    assert(queue);
    assert(submit_count == 0 || submits);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto queue_pointer = vulkan::Vulkan_device::Queue::from_handle(queue);
            for(std::size_t i = 0; i < submit_count; i++)
            {
                auto &submission = submits[i];
                assert(submission.sType == VK_STRUCTURE_TYPE_SUBMIT_INFO);
                struct Run_submission_job final : public vulkan::Vulkan_device::Job
                {
                    std::vector<vulkan::Vulkan_semaphore *> wait_semaphores;
                    std::vector<vulkan::Vulkan_command_buffer *> command_buffers;
                    std::vector<vulkan::Vulkan_semaphore *> signal_semaphores;
                    Run_submission_job(
                        std::vector<vulkan::Vulkan_semaphore *> wait_semaphores,
                        std::vector<vulkan::Vulkan_command_buffer *> command_buffers,
                        std::vector<vulkan::Vulkan_semaphore *> signal_semaphores) noexcept
                        : wait_semaphores(std::move(wait_semaphores)),
                          command_buffers(std::move(command_buffers)),
                          signal_semaphores(std::move(signal_semaphores))
                    {
                    }
                    virtual void run() noexcept override
                    {
                        for(auto &i : wait_semaphores)
                            i->wait();
                        for(auto &i : command_buffers)
                            i->run();
                        for(auto &i : signal_semaphores)
                            i->signal();
                    }
                };
                std::vector<vulkan::Vulkan_semaphore *> wait_semaphores;
                wait_semaphores.reserve(submission.waitSemaphoreCount);
                for(std::uint32_t i = 0; i < submission.waitSemaphoreCount; i++)
                {
                    assert(submission.pWaitSemaphores[i]);
                    wait_semaphores.push_back(
                        vulkan::Vulkan_semaphore::from_handle(submission.pWaitSemaphores[i]));
                }
                std::vector<vulkan::Vulkan_command_buffer *> command_buffers;
                command_buffers.reserve(submission.commandBufferCount);
                for(std::uint32_t i = 0; i < submission.commandBufferCount; i++)
                {
                    assert(submission.pCommandBuffers[i]);
                    command_buffers.push_back(
                        vulkan::Vulkan_command_buffer::from_handle(submission.pCommandBuffers[i]));
                }
                std::vector<vulkan::Vulkan_semaphore *> signal_semaphores;
                signal_semaphores.reserve(submission.signalSemaphoreCount);
                for(std::uint32_t i = 0; i < submission.signalSemaphoreCount; i++)
                {
                    assert(submission.pSignalSemaphores[i]);
                    signal_semaphores.push_back(
                        vulkan::Vulkan_semaphore::from_handle(submission.pSignalSemaphores[i]));
                }
                queue_pointer->queue_job(
                    std::make_unique<Run_submission_job>(std::move(wait_semaphores),
                                                         std::move(command_buffers),
                                                         std::move(signal_semaphores)));
            }
            if(fence)
                queue_pointer->queue_fence_signal(*vulkan::Vulkan_fence::from_handle(fence));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue queue)
{
    assert(queue);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto queue_pointer = vulkan::Vulkan_device::Queue::from_handle(queue);
            queue_pointer->wait_idle();
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device)
{
    assert(device);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto device_pointer = vulkan::Vulkan_device::from_handle(device);
            device_pointer->wait_idle();
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkAllocateMemory(VkDevice device,
                     const VkMemoryAllocateInfo *allocate_info,
                     const VkAllocationCallbacks *allocator,
                     VkDeviceMemory *memory)
{
    validate_allocator(allocator);
    assert(device);
    assert(allocate_info);
    assert(memory);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_device_memory::create(
                *vulkan::Vulkan_device::from_handle(device), *allocate_info);
            *memory = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice device,
                                                   VkDeviceMemory memory,
                                                   const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_device_memory::move_from_handle(memory).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(VkDevice device,
                                                      VkDeviceMemory memory,
                                                      VkDeviceSize offset,
                                                      VkDeviceSize size,
                                                      VkMemoryMapFlags flags,
                                                      void **data)
{
    assert(device);
    assert(memory);
    assert(data);
    *data = static_cast<unsigned char *>(
                vulkan::Vulkan_device_memory::from_handle(memory)->memory.get())
            + offset;
    return VK_SUCCESS;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(VkDevice device, VkDeviceMemory memory)
{
    assert(device);
    assert(memory);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkFlushMappedMemoryRanges(
    VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges)
{
#warning finish implementing vkFlushMappedMemoryRanges
    assert(!"vkFlushMappedMemoryRanges is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkInvalidateMappedMemoryRanges(
    VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange *pMemoryRanges)
{
#warning finish implementing vkInvalidateMappedMemoryRanges
    assert(!"vkInvalidateMappedMemoryRanges is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceMemoryCommitment(
    VkDevice device, VkDeviceMemory memory, VkDeviceSize *pCommittedMemoryInBytes)
{
#warning finish implementing vkGetDeviceMemoryCommitment
    assert(!"vkGetDeviceMemoryCommitment is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(VkDevice device,
                                                             VkBuffer buffer,
                                                             VkDeviceMemory memory,
                                                             VkDeviceSize memory_offset)
{
    assert(device);
    assert(buffer);
    assert(memory);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto *buffer_pointer = vulkan::Vulkan_buffer::from_handle(buffer);
            auto *device_memory = vulkan::Vulkan_device_memory::from_handle(memory);
            assert(!buffer_pointer->memory);
            buffer_pointer->memory = std::shared_ptr<void>(
                device_memory->memory,
                static_cast<unsigned char *>(device_memory->memory.get()) + memory_offset);
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory(VkDevice device,
                                                            VkImage image,
                                                            VkDeviceMemory memory,
                                                            VkDeviceSize memory_offset)
{
    assert(device);
    assert(image);
    assert(memory);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto *image_pointer = vulkan::Vulkan_image::from_handle(image);
            auto *device_memory = vulkan::Vulkan_device_memory::from_handle(memory);
            assert(!image_pointer->memory);
            image_pointer->memory = std::shared_ptr<void>(
                device_memory->memory,
                static_cast<unsigned char *>(device_memory->memory.get()) + memory_offset);
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements(
    VkDevice device, VkBuffer buffer, VkMemoryRequirements *memory_requirements)
{
    assert(device);
    assert(buffer);
    assert(memory_requirements);
    *memory_requirements =
        vulkan::Vulkan_buffer::from_handle(buffer)->descriptor.get_memory_requirements();
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(
    VkDevice device, VkImage image, VkMemoryRequirements *memory_requirements)
{
    assert(device);
    assert(image);
    assert(memory_requirements);
    *memory_requirements =
        vulkan::Vulkan_image::from_handle(image)->descriptor.get_memory_requirements();
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkGetImageSparseMemoryRequirements(VkDevice device,
                                       VkImage image,
                                       uint32_t *pSparseMemoryRequirementCount,
                                       VkSparseImageMemoryRequirements *pSparseMemoryRequirements)
{
#warning finish implementing vkGetImageSparseMemoryRequirements
    assert(!"vkGetImageSparseMemoryRequirements is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice,
                                                   VkFormat format,
                                                   VkImageType type,
                                                   VkSampleCountFlagBits samples,
                                                   VkImageUsageFlags usage,
                                                   VkImageTiling tiling,
                                                   uint32_t *pPropertyCount,
                                                   VkSparseImageFormatProperties *pProperties)
{
#warning finish implementing vkGetPhysicalDeviceSparseImageFormatProperties
    assert(!"vkGetPhysicalDeviceSparseImageFormatProperties is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueBindSparse(VkQueue queue,
                                                            uint32_t bindInfoCount,
                                                            const VkBindSparseInfo *pBindInfo,
                                                            VkFence fence)
{
#warning finish implementing vkQueueBindSparse
    assert(!"vkQueueBindSparse is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice device,
                                                        const VkFenceCreateInfo *create_info,
                                                        const VkAllocationCallbacks *allocator,
                                                        VkFence *fence)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(fence);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_fence::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *fence = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice device,
                                                     VkFence fence,
                                                     const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_fence::move_from_handle(fence).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice device,
                                                        uint32_t fence_count,
                                                        const VkFence *fences)
{
    assert(device);
    assert(fences);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            for(std::uint32_t i = 0; i < fence_count; i++)
                vulkan::Vulkan_fence::from_handle(fences[i])->reset();
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(VkDevice device, VkFence fence)
{
    assert(device);
    assert(fence);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            return vulkan::Vulkan_fence::from_handle(fence)->is_signaled() ? VK_SUCCESS :
                                                                             VK_NOT_READY;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice device,
                                                          uint32_t fence_count,
                                                          const VkFence *fences,
                                                          VkBool32 wait_all,
                                                          uint64_t timeout)
{
    assert(device);
    assert(fences);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            return vulkan::Vulkan_fence::wait_multiple(fence_count, fences, wait_all, timeout);
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateSemaphore(VkDevice device,
                      const VkSemaphoreCreateInfo *create_info,
                      const VkAllocationCallbacks *allocator,
                      VkSemaphore *semaphore)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(semaphore);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_semaphore::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *semaphore = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySemaphore(VkDevice device,
                                                         VkSemaphore semaphore,
                                                         const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_semaphore::move_from_handle(semaphore).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateEvent(VkDevice device,
                                                        const VkEventCreateInfo *pCreateInfo,
                                                        const VkAllocationCallbacks *allocator,
                                                        VkEvent *pEvent)
{
    validate_allocator(allocator);
#warning finish implementing vkCreateEvent
    assert(!"vkCreateEvent is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyEvent(VkDevice device,
                                                     VkEvent event,
                                                     const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
#warning finish implementing vkDestroyEvent
    assert(!"vkDestroyEvent is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetEventStatus(VkDevice device, VkEvent event)
{
#warning finish implementing vkGetEventStatus
    assert(!"vkGetEventStatus is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkSetEvent(VkDevice device, VkEvent event)
{
#warning finish implementing vkSetEvent
    assert(!"vkSetEvent is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetEvent(VkDevice device, VkEvent event)
{
#warning finish implementing vkResetEvent
    assert(!"vkResetEvent is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateQueryPool(VkDevice device,
                      const VkQueryPoolCreateInfo *pCreateInfo,
                      const VkAllocationCallbacks *allocator,
                      VkQueryPool *pQueryPool)
{
#warning finish implementing vkCreateQueryPool
    assert(!"vkCreateQueryPool is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyQueryPool(VkDevice device,
                                                         VkQueryPool queryPool,
                                                         const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
#warning finish implementing vkDestroyQueryPool
    assert(!"vkDestroyQueryPool is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetQueryPoolResults(VkDevice device,
                                                                VkQueryPool queryPool,
                                                                uint32_t firstQuery,
                                                                uint32_t queryCount,
                                                                size_t dataSize,
                                                                void *pData,
                                                                VkDeviceSize stride,
                                                                VkQueryResultFlags flags)
{
#warning finish implementing vkGetQueryPoolResults
    assert(!"vkGetQueryPoolResults is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(VkDevice device,
                                                         const VkBufferCreateInfo *create_info,
                                                         const VkAllocationCallbacks *allocator,
                                                         VkBuffer *buffer)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(buffer);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_buffer::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *buffer = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(VkDevice device,
                                                      VkBuffer buffer,
                                                      const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_buffer::move_from_handle(buffer).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateBufferView(VkDevice device,
                       const VkBufferViewCreateInfo *pCreateInfo,
                       const VkAllocationCallbacks *allocator,
                       VkBufferView *pView)
{
    validate_allocator(allocator);
#warning finish implementing vkCreateBufferView
    assert(!"vkCreateBufferView is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyBufferView(VkDevice device,
                                                          VkBufferView bufferView,
                                                          const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
#warning finish implementing vkDestroyBufferView
    assert(!"vkDestroyBufferView is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice device,
                                                        const VkImageCreateInfo *create_info,
                                                        const VkAllocationCallbacks *allocator,
                                                        VkImage *image)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(image);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_image::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *image = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice device,
                                                     VkImage image,
                                                     const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_image::move_from_handle(image).reset();
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkGetImageSubresourceLayout(VkDevice device,
                                VkImage image,
                                const VkImageSubresource *pSubresource,
                                VkSubresourceLayout *pLayout)
{
#warning finish implementing vkGetImageSubresourceLayout
    assert(!"vkGetImageSubresourceLayout is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateImageView(VkDevice device,
                      const VkImageViewCreateInfo *create_info,
                      const VkAllocationCallbacks *allocator,
                      VkImageView *view)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(view);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_image_view::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *view = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice device,
                                                         VkImageView image_view,
                                                         const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_image_view::move_from_handle(image_view).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateShaderModule(VkDevice device,
                         const VkShaderModuleCreateInfo *create_info,
                         const VkAllocationCallbacks *allocator,
                         VkShaderModule *shader_module)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(shader_module);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = pipeline::Shader_module::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *shader_module = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice device,
                                                            VkShaderModule shader_module,
                                                            const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    pipeline::Shader_module::move_from_handle(shader_module).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreatePipelineCache(VkDevice device,
                          const VkPipelineCacheCreateInfo *create_info,
                          const VkAllocationCallbacks *allocator,
                          VkPipelineCache *pipeline_cache)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(pipeline_cache);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = pipeline::Pipeline_cache::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *pipeline_cache = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineCache(VkDevice device,
                                                             VkPipelineCache pipeline_cache,
                                                             const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    pipeline::Pipeline_cache::move_from_handle(pipeline_cache).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPipelineCacheData(VkDevice device,
                                                                 VkPipelineCache pipelineCache,
                                                                 size_t *pDataSize,
                                                                 void *pData)
{
#warning finish implementing vkGetPipelineCacheData
    assert(!"vkGetPipelineCacheData is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkMergePipelineCaches(VkDevice device,
                                                                VkPipelineCache dstCache,
                                                                uint32_t srcCacheCount,
                                                                const VkPipelineCache *pSrcCaches)
{
#warning finish implementing vkMergePipelineCaches
    assert(!"vkMergePipelineCaches is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateGraphicsPipelines(VkDevice device,
                              VkPipelineCache pipeline_cache,
                              uint32_t create_info_count,
                              const VkGraphicsPipelineCreateInfo *create_infos,
                              const VkAllocationCallbacks *allocator,
                              VkPipeline *pipelines)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info_count != 0);
    assert(create_infos);
    assert(pipelines);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            std::vector<std::unique_ptr<pipeline::Pipeline>> pipeline_vector;
            pipeline_vector.resize(create_info_count);
            for(std::uint32_t i = 0; i < create_info_count; i++)
                pipeline_vector[i] = pipeline::Graphics_pipeline::create(
                    *vulkan::Vulkan_device::from_handle(device),
                    pipeline::Pipeline_cache::from_handle(pipeline_cache),
                    create_infos[i]);
            // only copy to pipelines after we're sure nothing will throw
            for(std::uint32_t i = 0; i < create_info_count; i++)
                pipelines[i] = move_to_handle(std::move(pipeline_vector[i]));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateComputePipelines(VkDevice device,
                             VkPipelineCache pipelineCache,
                             uint32_t createInfoCount,
                             const VkComputePipelineCreateInfo *pCreateInfos,
                             const VkAllocationCallbacks *allocator,
                             VkPipeline *pPipelines)
{
    validate_allocator(allocator);
#warning finish implementing vkCreateComputePipelines
    assert(!"vkCreateComputePipelines is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice device,
                                                        VkPipeline pipeline,
                                                        const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    pipeline::Pipeline::move_from_handle(pipeline).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreatePipelineLayout(VkDevice device,
                           const VkPipelineLayoutCreateInfo *create_info,
                           const VkAllocationCallbacks *allocator,
                           VkPipelineLayout *pipeline_layout)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(pipeline_layout);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_pipeline_layout::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *pipeline_layout = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(
    VkDevice device, VkPipelineLayout pipeline_layout, const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_pipeline_layout::move_from_handle(pipeline_layout).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSampler(VkDevice device,
                                                          const VkSamplerCreateInfo *pCreateInfo,
                                                          const VkAllocationCallbacks *allocator,
                                                          VkSampler *pSampler)
{
    validate_allocator(allocator);
#warning finish implementing vkCreateSampler
    assert(!"vkCreateSampler is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySampler(VkDevice device,
                                                       VkSampler sampler,
                                                       const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_sampler::move_from_handle(sampler).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateDescriptorSetLayout(VkDevice device,
                                const VkDescriptorSetLayoutCreateInfo *create_info,
                                const VkAllocationCallbacks *allocator,
                                VkDescriptorSetLayout *set_layout)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(set_layout);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_descriptor_set_layout::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *set_layout = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkDestroyDescriptorSetLayout(VkDevice device,
                                 VkDescriptorSetLayout descriptor_set_layout,
                                 const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_descriptor_set_layout::move_from_handle(descriptor_set_layout).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateDescriptorPool(VkDevice device,
                           const VkDescriptorPoolCreateInfo *pCreateInfo,
                           const VkAllocationCallbacks *allocator,
                           VkDescriptorPool *pDescriptorPool)
{
    validate_allocator(allocator);
#warning finish implementing vkCreateDescriptorPool
    assert(!"vkCreateDescriptorPool is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(
    VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
#warning finish implementing vkDestroyDescriptorPool
    assert(!"vkDestroyDescriptorPool is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetDescriptorPool(VkDevice device,
                                                                VkDescriptorPool descriptorPool,
                                                                VkDescriptorPoolResetFlags flags)
{
#warning finish implementing vkResetDescriptorPool
    assert(!"vkResetDescriptorPool is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkAllocateDescriptorSets(VkDevice device,
                             const VkDescriptorSetAllocateInfo *pAllocateInfo,
                             VkDescriptorSet *pDescriptorSets)
{
#warning finish implementing vkAllocateDescriptorSets
    assert(!"vkAllocateDescriptorSets is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkFreeDescriptorSets(VkDevice device,
                         VkDescriptorPool descriptorPool,
                         uint32_t descriptorSetCount,
                         const VkDescriptorSet *pDescriptorSets)
{
#warning finish implementing vkFreeDescriptorSets
    assert(!"vkFreeDescriptorSets is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkUpdateDescriptorSets(VkDevice device,
                           uint32_t descriptorWriteCount,
                           const VkWriteDescriptorSet *pDescriptorWrites,
                           uint32_t descriptorCopyCount,
                           const VkCopyDescriptorSet *pDescriptorCopies)
{
#warning finish implementing vkUpdateDescriptorSets
    assert(!"vkUpdateDescriptorSets is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateFramebuffer(VkDevice device,
                        const VkFramebufferCreateInfo *create_info,
                        const VkAllocationCallbacks *allocator,
                        VkFramebuffer *framebuffer)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(framebuffer);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_framebuffer::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *framebuffer = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyFramebuffer(VkDevice device,
                                                           VkFramebuffer framebuffer,
                                                           const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_framebuffer::move_from_handle(framebuffer).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateRenderPass(VkDevice device,
                       const VkRenderPassCreateInfo *create_info,
                       const VkAllocationCallbacks *allocator,
                       VkRenderPass *render_pass)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(render_pass);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_render_pass::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *render_pass = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyRenderPass(VkDevice device,
                                                          VkRenderPass render_pass,
                                                          const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    vulkan::Vulkan_render_pass::move_from_handle(render_pass).reset();
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetRenderAreaGranularity(VkDevice device,
                                                                 VkRenderPass renderPass,
                                                                 VkExtent2D *pGranularity)
{
#warning finish implementing vkGetRenderAreaGranularity
    assert(!"vkGetRenderAreaGranularity is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateCommandPool(VkDevice device,
                        const VkCommandPoolCreateInfo *create_info,
                        const VkAllocationCallbacks *allocator,
                        VkCommandPool *command_pool)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(command_pool);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_command_pool::create(
                *vulkan::Vulkan_device::from_handle(device), *create_info);
            *command_pool = move_to_handle(std::move(create_result));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice device,
                                                           VkCommandPool command_pool,
                                                           const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    assert(command_pool);
    vulkan::Vulkan_command_pool::move_from_handle(command_pool).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandPool(VkDevice device,
                                                             VkCommandPool command_pool,
                                                             VkCommandPoolResetFlags flags)
{
    assert(device);
    assert(command_pool);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            vulkan::Vulkan_command_pool::from_handle(command_pool)->reset(flags);
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkAllocateCommandBuffers(VkDevice device,
                             const VkCommandBufferAllocateInfo *allocate_info,
                             VkCommandBuffer *command_buffers)
{
    assert(device);
    assert(allocate_info);
    assert(command_buffers);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            vulkan::Vulkan_command_pool::from_handle(allocate_info->commandPool)
                ->allocate_multiple(
                    *vulkan::Vulkan_device::from_handle(device), *allocate_info, command_buffers);
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(VkDevice device,
                                                           VkCommandPool command_pool,
                                                           uint32_t command_buffer_count,
                                                           const VkCommandBuffer *command_buffers)
{
    assert(device);
    assert(command_pool);
    assert(command_buffers);
    vulkan::Vulkan_command_pool::from_handle(command_pool)
        ->free_multiple(command_buffers, command_buffer_count);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkBeginCommandBuffer(VkCommandBuffer command_buffer, const VkCommandBufferBeginInfo *begin_info)
{
    assert(command_buffer);
    assert(begin_info);
    assert(begin_info->sType == VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            vulkan::Vulkan_command_buffer::from_handle(command_buffer)->begin(*begin_info);
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer command_buffer)
{
    assert(command_buffer);
    return vulkan::Vulkan_command_buffer::from_handle(command_buffer)->end();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer command_buffer,
                                                               VkCommandBufferResetFlags flags)
{
    assert(command_buffer);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            vulkan::Vulkan_command_buffer::from_handle(command_buffer)->reset(flags);
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                                        VkPipelineBindPoint pipelineBindPoint,
                                                        VkPipeline pipeline)
{
#warning finish implementing vkCmdBindPipeline
    assert(!"vkCmdBindPipeline is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer commandBuffer,
                                                       uint32_t firstViewport,
                                                       uint32_t viewportCount,
                                                       const VkViewport *pViewports)
{
#warning finish implementing vkCmdSetViewport
    assert(!"vkCmdSetViewport is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer commandBuffer,
                                                      uint32_t firstScissor,
                                                      uint32_t scissorCount,
                                                      const VkRect2D *pScissors)
{
#warning finish implementing vkCmdSetScissor
    assert(!"vkCmdSetScissor is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetLineWidth(VkCommandBuffer commandBuffer,
                                                        float lineWidth)
{
#warning finish implementing vkCmdSetLineWidth
    assert(!"vkCmdSetLineWidth is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBias(VkCommandBuffer commandBuffer,
                                                        float depthBiasConstantFactor,
                                                        float depthBiasClamp,
                                                        float depthBiasSlopeFactor)
{
#warning finish implementing vkCmdSetDepthBias
    assert(!"vkCmdSetDepthBias is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetBlendConstants(VkCommandBuffer commandBuffer,
                                                             const float blendConstants[4])
{
#warning finish implementing vkCmdSetBlendConstants
    assert(!"vkCmdSetBlendConstants is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetDepthBounds(VkCommandBuffer commandBuffer,
                                                          float minDepthBounds,
                                                          float maxDepthBounds)
{
#warning finish implementing vkCmdSetDepthBounds
    assert(!"vkCmdSetDepthBounds is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer,
                                                                 VkStencilFaceFlags faceMask,
                                                                 uint32_t compareMask)
{
#warning finish implementing vkCmdSetStencilCompareMask
    assert(!"vkCmdSetStencilCompareMask is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer,
                                                               VkStencilFaceFlags faceMask,
                                                               uint32_t writeMask)
{
#warning finish implementing vkCmdSetStencilWriteMask
    assert(!"vkCmdSetStencilWriteMask is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetStencilReference(VkCommandBuffer commandBuffer,
                                                               VkStencilFaceFlags faceMask,
                                                               uint32_t reference)
{
#warning finish implementing vkCmdSetStencilReference
    assert(!"vkCmdSetStencilReference is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer,
                            VkPipelineBindPoint pipelineBindPoint,
                            VkPipelineLayout layout,
                            uint32_t firstSet,
                            uint32_t descriptorSetCount,
                            const VkDescriptorSet *pDescriptorSets,
                            uint32_t dynamicOffsetCount,
                            const uint32_t *pDynamicOffsets)
{
#warning finish implementing vkCmdBindDescriptorSets
    assert(!"vkCmdBindDescriptorSets is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer,
                                                           VkBuffer buffer,
                                                           VkDeviceSize offset,
                                                           VkIndexType indexType)
{
#warning finish implementing vkCmdBindIndexBuffer
    assert(!"vkCmdBindIndexBuffer is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer,
                                                             uint32_t firstBinding,
                                                             uint32_t bindingCount,
                                                             const VkBuffer *pBuffers,
                                                             const VkDeviceSize *pOffsets)
{
#warning finish implementing vkCmdBindVertexBuffers
    assert(!"vkCmdBindVertexBuffers is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer commandBuffer,
                                                uint32_t vertexCount,
                                                uint32_t instanceCount,
                                                uint32_t firstVertex,
                                                uint32_t firstInstance)
{
#warning finish implementing vkCmdDraw
    assert(!"vkCmdDraw is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexed(VkCommandBuffer commandBuffer,
                                                       uint32_t indexCount,
                                                       uint32_t instanceCount,
                                                       uint32_t firstIndex,
                                                       int32_t vertexOffset,
                                                       uint32_t firstInstance)
{
#warning finish implementing vkCmdDrawIndexed
    assert(!"vkCmdDrawIndexed is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndirect(VkCommandBuffer commandBuffer,
                                                        VkBuffer buffer,
                                                        VkDeviceSize offset,
                                                        uint32_t drawCount,
                                                        uint32_t stride)
{
#warning finish implementing vkCmdDrawIndirect
    assert(!"vkCmdDrawIndirect is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer,
                                                               VkBuffer buffer,
                                                               VkDeviceSize offset,
                                                               uint32_t drawCount,
                                                               uint32_t stride)
{
#warning finish implementing vkCmdDrawIndexedIndirect
    assert(!"vkCmdDrawIndexedIndirect is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDispatch(VkCommandBuffer commandBuffer,
                                                    uint32_t groupCountX,
                                                    uint32_t groupCountY,
                                                    uint32_t groupCountZ)
{
#warning finish implementing vkCmdDispatch
    assert(!"vkCmdDispatch is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdDispatchIndirect(VkCommandBuffer commandBuffer,
                                                            VkBuffer buffer,
                                                            VkDeviceSize offset)
{
#warning finish implementing vkCmdDispatchIndirect
    assert(!"vkCmdDispatchIndirect is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdCopyBuffer(VkCommandBuffer command_buffer,
                                                      VkBuffer src_buffer,
                                                      VkBuffer dst_buffer,
                                                      uint32_t region_count,
                                                      const VkBufferCopy *regions)
{
    assert(command_buffer);
    assert(src_buffer);
    assert(dst_buffer);
    assert(region_count > 0);
    assert(regions);
    auto command_buffer_pointer = vulkan::Vulkan_command_buffer::from_handle(command_buffer);
    command_buffer_pointer->record_command_and_keep_errors(
        [&]()
        {
            auto src_buffer_pointer = vulkan::Vulkan_buffer::from_handle(src_buffer);
            auto dst_buffer_pointer = vulkan::Vulkan_buffer::from_handle(dst_buffer);
            for(std::uint32_t i = 0; i < region_count; i++)
            {
                auto &region = regions[i];
                assert(region.size <= src_buffer_pointer->descriptor.size);
                assert(src_buffer_pointer->descriptor.size - region.size >= region.srcOffset);
                assert(region.size <= dst_buffer_pointer->descriptor.size);
                assert(dst_buffer_pointer->descriptor.size - region.size >= region.dstOffset);
                static_cast<void>(region);
            }
            struct Copy_buffer_command final : public vulkan::Vulkan_command_buffer::Command
            {
                vulkan::Vulkan_buffer &src_buffer;
                vulkan::Vulkan_buffer &dst_buffer;
                std::vector<VkBufferCopy> regions;
                Copy_buffer_command(vulkan::Vulkan_buffer &src_buffer,
                                    vulkan::Vulkan_buffer &dst_buffer,
                                    std::vector<VkBufferCopy> regions) noexcept
                    : src_buffer(src_buffer),
                      dst_buffer(dst_buffer),
                      regions(std::move(regions))
                {
                }
                virtual void run(
                    vulkan::Vulkan_command_buffer::Running_state &state) noexcept override
                {
                    static_cast<void>(state);
                    for(auto &region : regions)
                        std::memcpy(static_cast<unsigned char *>(dst_buffer.memory.get())
                                        + region.dstOffset,
                                    static_cast<const unsigned char *>(src_buffer.memory.get())
                                        + region.srcOffset,
                                    region.size);
                }
            };
            command_buffer_pointer->commands.push_back(std::make_unique<Copy_buffer_command>(
                *src_buffer_pointer,
                *dst_buffer_pointer,
                std::vector<VkBufferCopy>(regions, regions + region_count)));
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage(VkCommandBuffer commandBuffer,
                                                     VkImage srcImage,
                                                     VkImageLayout srcImageLayout,
                                                     VkImage dstImage,
                                                     VkImageLayout dstImageLayout,
                                                     uint32_t regionCount,
                                                     const VkImageCopy *pRegions)
{
#warning finish implementing vkCmdCopyImage
    assert(!"vkCmdCopyImage is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBlitImage(VkCommandBuffer commandBuffer,
                                                     VkImage srcImage,
                                                     VkImageLayout srcImageLayout,
                                                     VkImage dstImage,
                                                     VkImageLayout dstImageLayout,
                                                     uint32_t regionCount,
                                                     const VkImageBlit *pRegions,
                                                     VkFilter filter)
{
#warning finish implementing vkCmdBlitImage
    assert(!"vkCmdBlitImage is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer,
                                                             VkBuffer srcBuffer,
                                                             VkImage dstImage,
                                                             VkImageLayout dstImageLayout,
                                                             uint32_t regionCount,
                                                             const VkBufferImageCopy *pRegions)
{
#warning finish implementing vkCmdCopyBufferToImage
    assert(!"vkCmdCopyBufferToImage is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer,
                                                             VkImage srcImage,
                                                             VkImageLayout srcImageLayout,
                                                             VkBuffer dstBuffer,
                                                             uint32_t regionCount,
                                                             const VkBufferImageCopy *pRegions)
{
#warning finish implementing vkCmdCopyImageToBuffer
    assert(!"vkCmdCopyImageToBuffer is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdUpdateBuffer(VkCommandBuffer commandBuffer,
                                                        VkBuffer dstBuffer,
                                                        VkDeviceSize dstOffset,
                                                        VkDeviceSize dataSize,
                                                        const void *pData)
{
#warning finish implementing vkCmdUpdateBuffer
    assert(!"vkCmdUpdateBuffer is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdFillBuffer(VkCommandBuffer commandBuffer,
                                                      VkBuffer dstBuffer,
                                                      VkDeviceSize dstOffset,
                                                      VkDeviceSize size,
                                                      uint32_t data)
{
#warning finish implementing vkCmdFillBuffer
    assert(!"vkCmdFillBuffer is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdClearColorImage(VkCommandBuffer command_buffer,
                                                           VkImage image,
                                                           VkImageLayout image_layout,
                                                           const VkClearColorValue *color,
                                                           uint32_t range_count,
                                                           const VkImageSubresourceRange *ranges)
{
    assert(command_buffer);
    assert(image);
    assert(image_layout == VK_IMAGE_LAYOUT_GENERAL
           || image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    assert(color);
    assert(range_count > 0);
    assert(ranges);
    auto command_buffer_pointer = vulkan::Vulkan_command_buffer::from_handle(command_buffer);
    command_buffer_pointer->record_command_and_keep_errors(
        [&]()
        {
            auto image_pointer = vulkan::Vulkan_image::from_handle(image);
            assert(range_count == 1
                   && "vkCmdClearColorImage with multiple subresource ranges is not implemented");
            for(std::uint32_t i = 0; i < range_count; i++)
            {
                auto &range = ranges[i];
                assert(range.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
                assert(range.baseMipLevel == 0
                       && (range.levelCount == image_pointer->descriptor.mip_levels
                           || range.levelCount == VK_REMAINING_MIP_LEVELS)
                       && "vkCmdClearColorImage with clearing only some of the mipmap levels is not implemented");
                assert(range.baseArrayLayer == 0
                       && (range.layerCount == image_pointer->descriptor.array_layers
                           || range.layerCount == VK_REMAINING_ARRAY_LAYERS)
                       && "vkCmdClearColorImage with clearing only some of the array layers is not implemented");
                static_cast<void>(range);
            }
#warning finish implementing non-linear image layouts
            struct Clear_command final : public vulkan::Vulkan_command_buffer::Command
            {
                VkClearColorValue clear_color;
                vulkan::Vulkan_image *image;
                Clear_command(const VkClearColorValue &clear_color,
                              vulkan::Vulkan_image *image) noexcept : clear_color(clear_color),
                                                                      image(image)
                {
                }
                virtual void run(
                    vulkan::Vulkan_command_buffer::Running_state &state) noexcept override
                {
                    static_cast<void>(state);
                    image->clear(clear_color);
                }
            };
            command_buffer_pointer->commands.push_back(
                std::make_unique<Clear_command>(*color, image_pointer));
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer,
                                VkImage image,
                                VkImageLayout imageLayout,
                                const VkClearDepthStencilValue *pDepthStencil,
                                uint32_t rangeCount,
                                const VkImageSubresourceRange *pRanges)
{
#warning finish implementing vkCmdClearDepthStencilImage
    assert(!"vkCmdClearDepthStencilImage is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdClearAttachments(VkCommandBuffer commandBuffer,
                                                            uint32_t attachmentCount,
                                                            const VkClearAttachment *pAttachments,
                                                            uint32_t rectCount,
                                                            const VkClearRect *pRects)
{
#warning finish implementing vkCmdClearAttachments
    assert(!"vkCmdClearAttachments is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdResolveImage(VkCommandBuffer commandBuffer,
                                                        VkImage srcImage,
                                                        VkImageLayout srcImageLayout,
                                                        VkImage dstImage,
                                                        VkImageLayout dstImageLayout,
                                                        uint32_t regionCount,
                                                        const VkImageResolve *pRegions)
{
#warning finish implementing vkCmdResolveImage
    assert(!"vkCmdResolveImage is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdSetEvent(VkCommandBuffer commandBuffer,
                                                    VkEvent event,
                                                    VkPipelineStageFlags stageMask)
{
#warning finish implementing vkCmdSetEvent
    assert(!"vkCmdSetEvent is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdResetEvent(VkCommandBuffer commandBuffer,
                                                      VkEvent event,
                                                      VkPipelineStageFlags stageMask)
{
#warning finish implementing vkCmdResetEvent
    assert(!"vkCmdResetEvent is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkCmdWaitEvents(VkCommandBuffer commandBuffer,
                    uint32_t eventCount,
                    const VkEvent *pEvents,
                    VkPipelineStageFlags srcStageMask,
                    VkPipelineStageFlags dstStageMask,
                    uint32_t memoryBarrierCount,
                    const VkMemoryBarrier *pMemoryBarriers,
                    uint32_t bufferMemoryBarrierCount,
                    const VkBufferMemoryBarrier *pBufferMemoryBarriers,
                    uint32_t imageMemoryBarrierCount,
                    const VkImageMemoryBarrier *pImageMemoryBarriers)
{
#warning finish implementing vkCmdWaitEvents
    assert(!"vkCmdWaitEvents is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkCmdPipelineBarrier(VkCommandBuffer command_buffer,
                         VkPipelineStageFlags src_stage_mask,
                         VkPipelineStageFlags dst_stage_mask,
                         VkDependencyFlags dependency_flags,
                         uint32_t memory_barrier_count,
                         const VkMemoryBarrier *memory_barriers,
                         uint32_t buffer_memory_barrier_count,
                         const VkBufferMemoryBarrier *buffer_memory_barriers,
                         uint32_t image_memory_barrier_count,
                         const VkImageMemoryBarrier *image_memory_barriers)
{
    assert(command_buffer);
    assert(src_stage_mask != 0);
    assert(dst_stage_mask != 0);
    assert((dependency_flags & ~VK_DEPENDENCY_BY_REGION_BIT) == 0);
    assert(memory_barrier_count == 0 || memory_barriers);
    assert(buffer_memory_barrier_count == 0 || buffer_memory_barriers);
    assert(image_memory_barrier_count == 0 || image_memory_barriers);
    auto command_buffer_pointer = vulkan::Vulkan_command_buffer::from_handle(command_buffer);
    command_buffer_pointer->record_command_and_keep_errors(
        [&]()
        {
            bool any_memory_barriers = false;
            for(std::uint32_t i = 0; i < memory_barrier_count; i++)
            {
                auto &memory_barrier = memory_barriers[i];
                assert(memory_barrier.sType == VK_STRUCTURE_TYPE_MEMORY_BARRIER);
#warning finish implementing vkCmdPipelineBarrier for VkMemoryBarrier
                assert(!"vkCmdPipelineBarrier for VkMemoryBarrier is not implemented");
                any_memory_barriers = true;
            }
            for(std::uint32_t i = 0; i < buffer_memory_barrier_count; i++)
            {
                auto &buffer_memory_barrier = buffer_memory_barriers[i];
                assert(buffer_memory_barrier.sType == VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER);
#warning finish implementing vkCmdPipelineBarrier for VkBufferMemoryBarrier
                assert(!"vkCmdPipelineBarrier for VkBufferMemoryBarrier is not implemented");
                any_memory_barriers = true;
            }
            for(std::uint32_t i = 0; i < image_memory_barrier_count; i++)
            {
                auto &image_memory_barrier = image_memory_barriers[i];
                assert(image_memory_barrier.sType == VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
#warning finish implementing non-linear image layouts
                any_memory_barriers = true;
            }
            if(any_memory_barriers)
            {
                struct Generic_memory_barrier_command final
                    : public vulkan::Vulkan_command_buffer::Command
                {
                    void run(kazan::vulkan::Vulkan_command_buffer::Running_state
                                 &state) noexcept override
                    {
                        static_cast<void>(state);
                        std::atomic_thread_fence(std::memory_order_acq_rel);
                    }
                };
                command_buffer_pointer->commands.push_back(
                    std::make_unique<Generic_memory_barrier_command>());
            }
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBeginQuery(VkCommandBuffer commandBuffer,
                                                      VkQueryPool queryPool,
                                                      uint32_t query,
                                                      VkQueryControlFlags flags)
{
#warning finish implementing vkCmdBeginQuery
    assert(!"vkCmdBeginQuery is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdEndQuery(VkCommandBuffer commandBuffer,
                                                    VkQueryPool queryPool,
                                                    uint32_t query)
{
#warning finish implementing vkCmdEndQuery
    assert(!"vkCmdEndQuery is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdResetQueryPool(VkCommandBuffer commandBuffer,
                                                          VkQueryPool queryPool,
                                                          uint32_t firstQuery,
                                                          uint32_t queryCount)
{
#warning finish implementing vkCmdResetQueryPool
    assert(!"vkCmdResetQueryPool is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdWriteTimestamp(VkCommandBuffer commandBuffer,
                                                          VkPipelineStageFlagBits pipelineStage,
                                                          VkQueryPool queryPool,
                                                          uint32_t query)
{
#warning finish implementing vkCmdWriteTimestamp
    assert(!"vkCmdWriteTimestamp is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer,
                                                                VkQueryPool queryPool,
                                                                uint32_t firstQuery,
                                                                uint32_t queryCount,
                                                                VkBuffer dstBuffer,
                                                                VkDeviceSize dstOffset,
                                                                VkDeviceSize stride,
                                                                VkQueryResultFlags flags)
{
#warning finish implementing vkCmdCopyQueryPoolResults
    assert(!"vkCmdCopyQueryPoolResults is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer commandBuffer,
                                                         VkPipelineLayout layout,
                                                         VkShaderStageFlags stageFlags,
                                                         uint32_t offset,
                                                         uint32_t size,
                                                         const void *pValues)
{
#warning finish implementing vkCmdPushConstants
    assert(!"vkCmdPushConstants is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL
    vkCmdBeginRenderPass(VkCommandBuffer commandBuffer,
                         const VkRenderPassBeginInfo *pRenderPassBegin,
                         VkSubpassContents contents)
{
#warning finish implementing vkCmdBeginRenderPass
    assert(!"vkCmdBeginRenderPass is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdNextSubpass(VkCommandBuffer commandBuffer,
                                                       VkSubpassContents contents)
{
#warning finish implementing vkCmdNextSubpass
    assert(!"vkCmdNextSubpass is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commandBuffer)
{
#warning finish implementing vkCmdEndRenderPass
    assert(!"vkCmdEndRenderPass is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdExecuteCommands(VkCommandBuffer commandBuffer,
                                                           uint32_t commandBufferCount,
                                                           const VkCommandBuffer *pCommandBuffers)
{
#warning finish implementing vkCmdExecuteCommands
    assert(!"vkCmdExecuteCommands is not implemented");
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance,
                                                          VkSurfaceKHR surface,
                                                          const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
#warning finish implementing vkDestroySurfaceKHR
    assert(!"vkDestroySurfaceKHR is not implemented");
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physical_device,
                                         uint32_t queue_family_index,
                                         VkSurfaceKHR surface,
                                         VkBool32 *supported)
{
    assert(physical_device);
    assert(queue_family_index < vulkan::Vulkan_physical_device::queue_family_property_count);
    assert(surface);
    assert(supported);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto *surface_base = reinterpret_cast<VkIcdSurfaceBase *>(surface);
            auto *wsi = vulkan_icd::Wsi::find(surface_base->platform);
            assert(wsi);
            bool is_supported{};
            auto result = wsi->get_surface_support(surface_base, is_supported);
            if(result != VK_SUCCESS)
                return result;
            *supported = is_supported;
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physical_device,
                                              VkSurfaceKHR surface,
                                              VkSurfaceCapabilitiesKHR *surface_capabilities)
{
    assert(physical_device);
    assert(surface);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto *surface_base = reinterpret_cast<VkIcdSurfaceBase *>(surface);
            auto *wsi = vulkan_icd::Wsi::find(surface_base->platform);
            assert(wsi);
            VkSurfaceCapabilitiesKHR capabilities{};
            auto result = wsi->get_surface_capabilities(surface_base, capabilities);
            if(result != VK_SUCCESS)
                return result;
            *surface_capabilities = capabilities;
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physical_device,
                                         VkSurfaceKHR surface,
                                         uint32_t *surface_format_count,
                                         VkSurfaceFormatKHR *surface_formats)
{
    assert(physical_device);
    assert(surface);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto *surface_base = reinterpret_cast<VkIcdSurfaceBase *>(surface);
            auto *wsi = vulkan_icd::Wsi::find(surface_base->platform);
            assert(wsi);
            std::vector<VkSurfaceFormatKHR> surface_formats_vector;
            auto result = wsi->get_surface_formats(surface_base, surface_formats_vector);
            if(result != VK_SUCCESS)
                return result;
            return vulkan_icd::vulkan_enumerate_list_helper(surface_format_count,
                                                            surface_formats,
                                                            surface_formats_vector.data(),
                                                            surface_formats_vector.size());
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice physical_device,
                                              VkSurfaceKHR surface,
                                              uint32_t *present_mode_count,
                                              VkPresentModeKHR *present_modes)
{
    assert(physical_device);
    assert(surface);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto *surface_base = reinterpret_cast<VkIcdSurfaceBase *>(surface);
            auto *wsi = vulkan_icd::Wsi::find(surface_base->platform);
            assert(wsi);
            std::vector<VkPresentModeKHR> present_modes_vector;
            auto result = wsi->get_present_modes(surface_base, present_modes_vector);
            if(result != VK_SUCCESS)
                return result;
            return vulkan_icd::vulkan_enumerate_list_helper(present_mode_count,
                                                            present_modes,
                                                            present_modes_vector.data(),
                                                            present_modes_vector.size());
        });
}

#ifdef VK_USE_PLATFORM_XCB_KHR
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateXcbSurfaceKHR(VkInstance instance,
                          const VkXcbSurfaceCreateInfoKHR *pCreateInfo,
                          const VkAllocationCallbacks *pAllocator,
                          VkSurfaceKHR *pSurface)
{
#warning finish implementing vkCreateXcbSurfaceKHR
    assert(!"vkCreateXcbSurfaceKHR is not implemented");
}

extern "C" VKAPI_ATTR VkBool32 VKAPI_CALL
    vkGetPhysicalDeviceXcbPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                                 uint32_t queueFamilyIndex,
                                                 xcb_connection_t *connection,
                                                 xcb_visualid_t visual_id)
{
#warning finish implementing vkGetPhysicalDeviceXcbPresentationSupportKHR
    assert(!"vkGetPhysicalDeviceXcbPresentationSupportKHR is not implemented");
}
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateXlibSurfaceKHR(VkInstance instance,
                           const VkXlibSurfaceCreateInfoKHR *pCreateInfo,
                           const VkAllocationCallbacks *pAllocator,
                           VkSurfaceKHR *pSurface)
{
#warning finish implementing vkCreateXlibSurfaceKHR
    assert(!"vkCreateXlibSurfaceKHR is not implemented");
}

extern "C" VKAPI_ATTR VkBool32 VKAPI_CALL vkGetPhysicalDeviceXlibPresentationSupportKHR(
    VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex, Display *dpy, VisualID visualID)
{
#warning finish implementing vkGetPhysicalDeviceXlibPresentationSupportKHR
    assert(!"vkGetPhysicalDeviceXlibPresentationSupportKHR is not implemented");
}
#endif

extern "C" VKAPI_ATTR VkResult VKAPI_CALL
    vkCreateSwapchainKHR(VkDevice device,
                         const VkSwapchainCreateInfoKHR *create_info,
                         const VkAllocationCallbacks *allocator,
                         VkSwapchainKHR *swapchain)
{
    validate_allocator(allocator);
    assert(device);
    assert(create_info);
    assert(swapchain);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            assert(create_info->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
            auto *surface_base = reinterpret_cast<VkIcdSurfaceBase *>(create_info->surface);
            auto *wsi = vulkan_icd::Wsi::find(surface_base->platform);
            assert(wsi);
            auto create_result =
                wsi->create_swapchain(*vulkan::Vulkan_device::from_handle(device), *create_info);
            if(util::holds_alternative<VkResult>(create_result))
                return util::get<VkResult>(create_result);
            *swapchain = move_to_handle(
                util::get<std::unique_ptr<vulkan_icd::Vulkan_swapchain>>(std::move(create_result)));
            return VK_SUCCESS;
        });
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device,
                                                            VkSwapchainKHR swapchain,
                                                            const VkAllocationCallbacks *allocator)
{
    validate_allocator(allocator);
    assert(device);
    assert(swapchain);
    vulkan_icd::Vulkan_swapchain::move_from_handle(swapchain).reset();
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice device,
                                                                  VkSwapchainKHR swapchain,
                                                                  uint32_t *swapchain_image_count,
                                                                  VkImage *swapchain_images)
{
    assert(device);
    assert(swapchain);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto *swapchain_pointer = vulkan_icd::Vulkan_swapchain::from_handle(swapchain);
            std::vector<VkImage> images;
            images.reserve(swapchain_pointer->images.size());
            for(auto &image : swapchain_pointer->images)
                images.push_back(to_handle(image.get()));
            return vulkan_icd::vulkan_enumerate_list_helper(
                swapchain_image_count, swapchain_images, images.data(), images.size());
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice device,
                                                                VkSwapchainKHR swapchain,
                                                                uint64_t timeout,
                                                                VkSemaphore semaphore,
                                                                VkFence fence,
                                                                uint32_t *image_index)
{
    assert(device);
    assert(swapchain);
    assert(image_index);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto *swapchain_pointer = vulkan_icd::Vulkan_swapchain::from_handle(swapchain);
            return swapchain_pointer->acquire_next_image(
                timeout,
                vulkan::Vulkan_semaphore::from_handle(semaphore),
                vulkan::Vulkan_fence::from_handle(fence),
                *image_index);
        });
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue,
                                                            const VkPresentInfoKHR *present_info)
{
    assert(queue);
    assert(present_info);
    assert(present_info->sType == VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
    assert(present_info->waitSemaphoreCount == 0 || present_info->pWaitSemaphores);
    assert(present_info->swapchainCount > 0);
    assert(present_info->pSwapchains);
    assert(present_info->pImageIndices);
    return vulkan_icd::catch_exceptions_and_return_result(
        [&]()
        {
            auto *queue_pointer = vulkan::Vulkan_device::Queue::from_handle(queue);
            if(present_info->waitSemaphoreCount > 0)
            {
                std::vector<vulkan::Vulkan_semaphore *> semaphores;
                semaphores.reserve(present_info->waitSemaphoreCount);
                for(std::uint32_t i = 0; i < present_info->waitSemaphoreCount; i++)
                {
                    assert(present_info->pWaitSemaphores[i]);
                    semaphores.push_back(
                        vulkan::Vulkan_semaphore::from_handle(present_info->pWaitSemaphores[i]));
                }
                struct Wait_for_semaphores_job final : public vulkan::Vulkan_device::Job
                {
                    std::vector<vulkan::Vulkan_semaphore *> semaphores;
                    explicit Wait_for_semaphores_job(
                        std::vector<vulkan::Vulkan_semaphore *> semaphores) noexcept
                        : semaphores(std::move(semaphores))
                    {
                    }
                    virtual void run() noexcept override
                    {
                        for(auto &i : semaphores)
                            i->wait();
                    }
                };
                queue_pointer->queue_job(
                    std::make_unique<Wait_for_semaphores_job>(std::move(semaphores)));
            }
            VkResult retval = VK_SUCCESS;
            for(std::uint32_t i = 0; i < present_info->swapchainCount; i++)
            {
                auto *swapchain =
                    vulkan_icd::Vulkan_swapchain::from_handle(present_info->pSwapchains[i]);
                assert(swapchain);
                VkResult present_result =
                    swapchain->queue_present(present_info->pImageIndices[i], *queue_pointer);
                if(present_result == VK_ERROR_DEVICE_LOST || retval == VK_ERROR_DEVICE_LOST)
                    retval = VK_ERROR_DEVICE_LOST;
                else if(present_result == VK_ERROR_SURFACE_LOST_KHR
                        || retval == VK_ERROR_SURFACE_LOST_KHR)
                    retval = VK_ERROR_SURFACE_LOST_KHR;
                else if(present_result == VK_ERROR_OUT_OF_DATE_KHR
                        || retval == VK_ERROR_OUT_OF_DATE_KHR)
                    retval = VK_ERROR_OUT_OF_DATE_KHR;
                else if(present_result == VK_SUBOPTIMAL_KHR || retval == VK_SUBOPTIMAL_KHR)
                    retval = VK_SUBOPTIMAL_KHR;
                if(present_info->pResults)
                    present_info->pResults[i] = present_result;
            }
            return retval;
        });
}

namespace kazan
{
namespace vulkan_icd
{
Vulkan_loader_interface *Vulkan_loader_interface::get() noexcept
{
    static Vulkan_loader_interface vulkan_loader_interface{};
    return &vulkan_loader_interface;
}

PFN_vkVoidFunction Vulkan_loader_interface::get_procedure_address(
    const char *name,
    Procedure_address_scope scope,
    vulkan::Vulkan_instance *instance,
    vulkan::Vulkan_device *device) noexcept
{
    using namespace util::string_view_literals;
    assert(name != "vkEnumerateInstanceLayerProperties"_sv
           && "shouldn't be called, implemented by the vulkan loader");
    assert(name != "vkEnumerateDeviceLayerProperties"_sv
           && "shouldn't be called, implemented by the vulkan loader");
    vulkan::Supported_extensions enabled_or_available_extensions{};
    switch(scope)
    {
    case Procedure_address_scope::Library:
        assert(!instance);
        assert(!device);
        // can't use any extension functions at library scope
        break;
    case Procedure_address_scope::Instance:
        assert(instance);
        assert(!device);
        enabled_or_available_extensions = instance->extensions;
        for(auto extension : util::Enum_traits<vulkan::Supported_extension>::values)
            if(vulkan::get_extension_scope(extension) == vulkan::Extension_scope::Device)
                enabled_or_available_extensions.insert(extension);
        break;
    case Procedure_address_scope::Device:
        assert(instance);
        assert(device);
        enabled_or_available_extensions = device->extensions;
        break;
    }

// INSTANCE_SCOPE_* is used for device scope functions as well

#define LIBRARY_SCOPE_FUNCTION(function_name)                     \
    do                                                            \
    {                                                             \
        if(name == #function_name##_sv)                           \
            return reinterpret_cast<PFN_vkVoidFunction>(          \
                static_cast<PFN_##function_name>(function_name)); \
    } while(0)
#define INSTANCE_SCOPE_FUNCTION(function_name)                                       \
    do                                                                               \
    {                                                                                \
        if(scope != Procedure_address_scope::Library && name == #function_name##_sv) \
            return reinterpret_cast<PFN_vkVoidFunction>(                             \
                static_cast<PFN_##function_name>(function_name));                    \
    } while(0)
#define INSTANCE_SCOPE_EXTENSION_FUNCTION(function_name, extension)                              \
    do                                                                                           \
    {                                                                                            \
        if(scope != Procedure_address_scope::Library                                             \
           && enabled_or_available_extensions.count(vulkan::Supported_extension::extension) != 0 \
           && name == #function_name##_sv)                                                       \
            return reinterpret_cast<PFN_vkVoidFunction>(                                         \
                static_cast<PFN_##function_name>(function_name));                                \
    } while(0)
// for functions implemented by the vulkan loader that shouldn't reach the ICD
#define LIBRARY_SCOPE_FUNCTION_IN_LOADER(function_name)                     \
    do                                                                      \
    {                                                                       \
        assert(name != #function_name##_sv                                  \
               && "shouldn't be called, implemented by the vulkan loader"); \
    } while(0)
#define INSTANCE_SCOPE_FUNCTION_IN_LOADER(function_name)                                  \
    do                                                                                    \
    {                                                                                     \
        assert((scope == Procedure_address_scope::Library || name != #function_name##_sv) \
               && "shouldn't be called, implemented by the vulkan loader");               \
    } while(0)
#define INSTANCE_SCOPE_EXTENSION_FUNCTION_IN_LOADER(function_name, extension)                      \
    do                                                                                             \
    {                                                                                              \
        assert(                                                                                    \
            (scope == Procedure_address_scope::Library                                             \
             || enabled_or_available_extensions.count(vulkan::Supported_extension::extension) == 0 \
             || name != #function_name##_sv)                                                       \
            && "shouldn't be called, implemented by the vulkan loader");                           \
    } while(0)

    LIBRARY_SCOPE_FUNCTION(vkEnumerateInstanceExtensionProperties);
    LIBRARY_SCOPE_FUNCTION(vkCreateInstance);
    LIBRARY_SCOPE_FUNCTION(vkEnumerateInstanceLayerProperties);
    INSTANCE_SCOPE_FUNCTION(vkDestroyInstance);
    INSTANCE_SCOPE_FUNCTION(vkEnumeratePhysicalDevices);
    INSTANCE_SCOPE_FUNCTION(vkGetPhysicalDeviceFeatures);
    INSTANCE_SCOPE_FUNCTION(vkGetPhysicalDeviceFormatProperties);
    INSTANCE_SCOPE_FUNCTION(vkGetPhysicalDeviceImageFormatProperties);
    INSTANCE_SCOPE_FUNCTION(vkGetPhysicalDeviceProperties);
    INSTANCE_SCOPE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);
    INSTANCE_SCOPE_FUNCTION(vkGetPhysicalDeviceMemoryProperties);
    INSTANCE_SCOPE_FUNCTION(vkGetInstanceProcAddr);
    INSTANCE_SCOPE_FUNCTION(vkGetDeviceProcAddr);
    INSTANCE_SCOPE_FUNCTION(vkCreateDevice);
    INSTANCE_SCOPE_FUNCTION(vkDestroyDevice);
    INSTANCE_SCOPE_FUNCTION(vkEnumerateDeviceExtensionProperties);
    INSTANCE_SCOPE_FUNCTION(vkEnumerateDeviceLayerProperties);
    INSTANCE_SCOPE_FUNCTION(vkGetDeviceQueue);
    INSTANCE_SCOPE_FUNCTION(vkQueueSubmit);
    INSTANCE_SCOPE_FUNCTION(vkQueueWaitIdle);
    INSTANCE_SCOPE_FUNCTION(vkDeviceWaitIdle);
    INSTANCE_SCOPE_FUNCTION(vkAllocateMemory);
    INSTANCE_SCOPE_FUNCTION(vkFreeMemory);
    INSTANCE_SCOPE_FUNCTION(vkMapMemory);
    INSTANCE_SCOPE_FUNCTION(vkUnmapMemory);
    INSTANCE_SCOPE_FUNCTION(vkFlushMappedMemoryRanges);
    INSTANCE_SCOPE_FUNCTION(vkInvalidateMappedMemoryRanges);
    INSTANCE_SCOPE_FUNCTION(vkGetDeviceMemoryCommitment);
    INSTANCE_SCOPE_FUNCTION(vkBindBufferMemory);
    INSTANCE_SCOPE_FUNCTION(vkBindImageMemory);
    INSTANCE_SCOPE_FUNCTION(vkGetBufferMemoryRequirements);
    INSTANCE_SCOPE_FUNCTION(vkGetImageMemoryRequirements);
    INSTANCE_SCOPE_FUNCTION(vkGetImageSparseMemoryRequirements);
    INSTANCE_SCOPE_FUNCTION(vkGetPhysicalDeviceSparseImageFormatProperties);
    INSTANCE_SCOPE_FUNCTION(vkQueueBindSparse);
    INSTANCE_SCOPE_FUNCTION(vkCreateFence);
    INSTANCE_SCOPE_FUNCTION(vkDestroyFence);
    INSTANCE_SCOPE_FUNCTION(vkResetFences);
    INSTANCE_SCOPE_FUNCTION(vkGetFenceStatus);
    INSTANCE_SCOPE_FUNCTION(vkWaitForFences);
    INSTANCE_SCOPE_FUNCTION(vkCreateSemaphore);
    INSTANCE_SCOPE_FUNCTION(vkDestroySemaphore);
    INSTANCE_SCOPE_FUNCTION(vkCreateEvent);
    INSTANCE_SCOPE_FUNCTION(vkDestroyEvent);
    INSTANCE_SCOPE_FUNCTION(vkGetEventStatus);
    INSTANCE_SCOPE_FUNCTION(vkSetEvent);
    INSTANCE_SCOPE_FUNCTION(vkResetEvent);
    INSTANCE_SCOPE_FUNCTION(vkCreateQueryPool);
    INSTANCE_SCOPE_FUNCTION(vkDestroyQueryPool);
    INSTANCE_SCOPE_FUNCTION(vkGetQueryPoolResults);
    INSTANCE_SCOPE_FUNCTION(vkCreateBuffer);
    INSTANCE_SCOPE_FUNCTION(vkDestroyBuffer);
    INSTANCE_SCOPE_FUNCTION(vkCreateBufferView);
    INSTANCE_SCOPE_FUNCTION(vkDestroyBufferView);
    INSTANCE_SCOPE_FUNCTION(vkCreateImage);
    INSTANCE_SCOPE_FUNCTION(vkDestroyImage);
    INSTANCE_SCOPE_FUNCTION(vkGetImageSubresourceLayout);
    INSTANCE_SCOPE_FUNCTION(vkCreateImageView);
    INSTANCE_SCOPE_FUNCTION(vkDestroyImageView);
    INSTANCE_SCOPE_FUNCTION(vkCreateShaderModule);
    INSTANCE_SCOPE_FUNCTION(vkDestroyShaderModule);
    INSTANCE_SCOPE_FUNCTION(vkCreatePipelineCache);
    INSTANCE_SCOPE_FUNCTION(vkDestroyPipelineCache);
    INSTANCE_SCOPE_FUNCTION(vkGetPipelineCacheData);
    INSTANCE_SCOPE_FUNCTION(vkMergePipelineCaches);
    INSTANCE_SCOPE_FUNCTION(vkCreateGraphicsPipelines);
    INSTANCE_SCOPE_FUNCTION(vkCreateComputePipelines);
    INSTANCE_SCOPE_FUNCTION(vkDestroyPipeline);
    INSTANCE_SCOPE_FUNCTION(vkCreatePipelineLayout);
    INSTANCE_SCOPE_FUNCTION(vkDestroyPipelineLayout);
    INSTANCE_SCOPE_FUNCTION(vkCreateSampler);
    INSTANCE_SCOPE_FUNCTION(vkDestroySampler);
    INSTANCE_SCOPE_FUNCTION(vkCreateDescriptorSetLayout);
    INSTANCE_SCOPE_FUNCTION(vkDestroyDescriptorSetLayout);
    INSTANCE_SCOPE_FUNCTION(vkCreateDescriptorPool);
    INSTANCE_SCOPE_FUNCTION(vkDestroyDescriptorPool);
    INSTANCE_SCOPE_FUNCTION(vkResetDescriptorPool);
    INSTANCE_SCOPE_FUNCTION(vkAllocateDescriptorSets);
    INSTANCE_SCOPE_FUNCTION(vkFreeDescriptorSets);
    INSTANCE_SCOPE_FUNCTION(vkUpdateDescriptorSets);
    INSTANCE_SCOPE_FUNCTION(vkCreateFramebuffer);
    INSTANCE_SCOPE_FUNCTION(vkDestroyFramebuffer);
    INSTANCE_SCOPE_FUNCTION(vkCreateRenderPass);
    INSTANCE_SCOPE_FUNCTION(vkDestroyRenderPass);
    INSTANCE_SCOPE_FUNCTION(vkGetRenderAreaGranularity);
    INSTANCE_SCOPE_FUNCTION(vkCreateCommandPool);
    INSTANCE_SCOPE_FUNCTION(vkDestroyCommandPool);
    INSTANCE_SCOPE_FUNCTION(vkResetCommandPool);
    INSTANCE_SCOPE_FUNCTION(vkAllocateCommandBuffers);
    INSTANCE_SCOPE_FUNCTION(vkFreeCommandBuffers);
    INSTANCE_SCOPE_FUNCTION(vkBeginCommandBuffer);
    INSTANCE_SCOPE_FUNCTION(vkEndCommandBuffer);
    INSTANCE_SCOPE_FUNCTION(vkResetCommandBuffer);
    INSTANCE_SCOPE_FUNCTION(vkCmdBindPipeline);
    INSTANCE_SCOPE_FUNCTION(vkCmdSetViewport);
    INSTANCE_SCOPE_FUNCTION(vkCmdSetScissor);
    INSTANCE_SCOPE_FUNCTION(vkCmdSetLineWidth);
    INSTANCE_SCOPE_FUNCTION(vkCmdSetDepthBias);
    INSTANCE_SCOPE_FUNCTION(vkCmdSetBlendConstants);
    INSTANCE_SCOPE_FUNCTION(vkCmdSetDepthBounds);
    INSTANCE_SCOPE_FUNCTION(vkCmdSetStencilCompareMask);
    INSTANCE_SCOPE_FUNCTION(vkCmdSetStencilWriteMask);
    INSTANCE_SCOPE_FUNCTION(vkCmdSetStencilReference);
    INSTANCE_SCOPE_FUNCTION(vkCmdBindDescriptorSets);
    INSTANCE_SCOPE_FUNCTION(vkCmdBindIndexBuffer);
    INSTANCE_SCOPE_FUNCTION(vkCmdBindVertexBuffers);
    INSTANCE_SCOPE_FUNCTION(vkCmdDraw);
    INSTANCE_SCOPE_FUNCTION(vkCmdDrawIndexed);
    INSTANCE_SCOPE_FUNCTION(vkCmdDrawIndirect);
    INSTANCE_SCOPE_FUNCTION(vkCmdDrawIndexedIndirect);
    INSTANCE_SCOPE_FUNCTION(vkCmdDispatch);
    INSTANCE_SCOPE_FUNCTION(vkCmdDispatchIndirect);
    INSTANCE_SCOPE_FUNCTION(vkCmdCopyBuffer);
    INSTANCE_SCOPE_FUNCTION(vkCmdCopyImage);
    INSTANCE_SCOPE_FUNCTION(vkCmdBlitImage);
    INSTANCE_SCOPE_FUNCTION(vkCmdCopyBufferToImage);
    INSTANCE_SCOPE_FUNCTION(vkCmdCopyImageToBuffer);
    INSTANCE_SCOPE_FUNCTION(vkCmdUpdateBuffer);
    INSTANCE_SCOPE_FUNCTION(vkCmdFillBuffer);
    INSTANCE_SCOPE_FUNCTION(vkCmdClearColorImage);
    INSTANCE_SCOPE_FUNCTION(vkCmdClearDepthStencilImage);
    INSTANCE_SCOPE_FUNCTION(vkCmdClearAttachments);
    INSTANCE_SCOPE_FUNCTION(vkCmdResolveImage);
    INSTANCE_SCOPE_FUNCTION(vkCmdSetEvent);
    INSTANCE_SCOPE_FUNCTION(vkCmdResetEvent);
    INSTANCE_SCOPE_FUNCTION(vkCmdWaitEvents);
    INSTANCE_SCOPE_FUNCTION(vkCmdPipelineBarrier);
    INSTANCE_SCOPE_FUNCTION(vkCmdBeginQuery);
    INSTANCE_SCOPE_FUNCTION(vkCmdEndQuery);
    INSTANCE_SCOPE_FUNCTION(vkCmdResetQueryPool);
    INSTANCE_SCOPE_FUNCTION(vkCmdWriteTimestamp);
    INSTANCE_SCOPE_FUNCTION(vkCmdCopyQueryPoolResults);
    INSTANCE_SCOPE_FUNCTION(vkCmdPushConstants);
    INSTANCE_SCOPE_FUNCTION(vkCmdBeginRenderPass);
    INSTANCE_SCOPE_FUNCTION(vkCmdNextSubpass);
    INSTANCE_SCOPE_FUNCTION(vkCmdEndRenderPass);
    INSTANCE_SCOPE_FUNCTION(vkCmdExecuteCommands);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkDestroySurfaceKHR, KHR_surface);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR, KHR_surface);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR, KHR_surface);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR, KHR_surface);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR, KHR_surface);
#ifdef VK_USE_PLATFORM_XCB_KHR
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkCreateXcbSurfaceKHR, KHR_xcb_surface);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkGetPhysicalDeviceXcbPresentationSupportKHR,
                                      KHR_xcb_surface);
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkCreateXlibSurfaceKHR, KHR_xlib_surface);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkGetPhysicalDeviceXlibPresentationSupportKHR,
                                      KHR_xlib_surface);
#endif
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkCreateSwapchainKHR, KHR_swapchain);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkDestroySwapchainKHR, KHR_swapchain);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkGetSwapchainImagesKHR, KHR_swapchain);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkAcquireNextImageKHR, KHR_swapchain);
    INSTANCE_SCOPE_EXTENSION_FUNCTION(vkQueuePresentKHR, KHR_swapchain);

#undef LIBRARY_SCOPE_FUNCTION
#undef INSTANCE_SCOPE_FUNCTION
#undef INSTANCE_SCOPE_EXTENSION_FUNCTION
    return nullptr;
}

PFN_vkVoidFunction Vulkan_loader_interface::get_instance_proc_addr(VkInstance instance,
                                                                   const char *name) noexcept
{
    if(!instance)
        return get_procedure_address(name, Procedure_address_scope::Library, nullptr, nullptr);
    return get_procedure_address(name,
                                 Procedure_address_scope::Instance,
                                 vulkan::Vulkan_instance::from_handle(instance),
                                 nullptr);
}

VkResult Vulkan_loader_interface::create_instance(const VkInstanceCreateInfo *create_info,
                                                  const VkAllocationCallbacks *allocator,
                                                  VkInstance *instance) noexcept
{
    validate_allocator(allocator);
    assert(create_info);
    assert(instance);
    return catch_exceptions_and_return_result(
        [&]()
        {
            auto create_result = vulkan::Vulkan_instance::create(*create_info);
            if(util::holds_alternative<VkResult>(create_result))
                return util::get<VkResult>(create_result);
            *instance = move_to_handle(
                util::get<std::unique_ptr<vulkan::Vulkan_instance>>(std::move(create_result)));
            return VK_SUCCESS;
        });
}

VkResult Vulkan_loader_interface::enumerate_instance_extension_properties(
    const char *layer_name, uint32_t *property_count, VkExtensionProperties *properties) noexcept
{
    assert(layer_name == nullptr);
    static constexpr auto extensions = vulkan::get_extensions<vulkan::Extension_scope::Instance>();
    return vulkan_enumerate_list_helper(
        property_count, properties, extensions.data(), extensions.size());
}

VkResult Vulkan_loader_interface::enumerate_device_extension_properties(
    VkPhysicalDevice physical_device,
    const char *layer_name,
    uint32_t *property_count,
    VkExtensionProperties *properties) noexcept
{
    assert(layer_name == nullptr);
    assert(physical_device != VK_NULL_HANDLE);
    static constexpr auto extensions = vulkan::get_extensions<vulkan::Extension_scope::Device>();
    return vulkan_enumerate_list_helper(
        property_count, properties, extensions.data(), extensions.size());
}
}

void vulkan_icd::print_exception(std::exception &e) noexcept
{
    std::cerr << "error: " << e.what() << std::endl;
}
}
