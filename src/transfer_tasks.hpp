#pragma once

#include <volk.h>

void TransferTask_Create(uint32_t max_frames_in_flight);
void TransferTask_Begin();
void TransferTask_End();
void TransferTask_Submit();
void TransferTask_NextFrame();
void TransferTask_Destroy();

VkSemaphore TransferTask_Semaphore();
uint64_t TransferTask_SemaphoreValue();
