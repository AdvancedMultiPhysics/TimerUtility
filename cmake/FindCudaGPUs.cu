#include <stdio.h>
#include <cuda_runtime.h>

int main() {
    int deviceCount, device;
    int gpuDeviceCount = 0;
    printf("0");

    struct cudaDeviceProp properties;
    cudaError_t cudaResultCode = cudaGetDeviceCount(&deviceCount);
    if (cudaResultCode != cudaSuccess) 
        deviceCount = 0;

    // machines with no GPUs can still report one emulation device
    for (device = 0; device < deviceCount; ++device) {
        cudaGetDeviceProperties(&properties, device);
        if (properties.major != 9999) // 9999 means emulation only
            ++gpuDeviceCount;
    }
    printf("%i\n", gpuDeviceCount);

    // don't return the number of gpus in case there are other errors
    return gpuDeviceCount > 0 ? 0:1;
}
