#include "transfer_tasks.hpp"
#include "graphics_tasks.hpp"
#include "compute_tasks.hpp"

#include "tasks.hpp"

static Task transfer_task = {};

void TransferTask_Create(uint32_t max_frames_in_flight)
{
	Task_Create(transfer_task, Device_Transfer_Queue(), PhysicalDeviceData_TransferQueueFamilyIndex(), max_frames_in_flight, "transfer task");
}

void TransferTask_Begin()
{
	Task_Start(transfer_task);
}

void TransferTask_End()
{
	Task_End(transfer_task);
}

void TransferTask_Submit()
{
	Task_Submit(transfer_task, {}, {});
}

void TransferTask_NextFrame()
{
	Task_NextFrame(transfer_task);
}

void TransferTask_Destroy()
{
	Task_Destroy(transfer_task);
}

VkSemaphore TransferTask_Semaphore()
{
	return Task_Semaphore(transfer_task);
}

uint64_t TransferTask_SemaphoreValue()
{
	return Task_SemaphoreValue(transfer_task);
}

