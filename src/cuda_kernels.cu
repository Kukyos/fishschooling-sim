#include <cuda_runtime.h>
#include <curand_kernel.h>
#include <stdio.h>

// Include Windows GL headers for OpenGL types
#ifdef _WIN32
    #include <windows.h>
    #include <GL/gl.h>
#endif

#include <cuda_gl_interop.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Instance data structure - matches OpenGL VBO layout
struct Instance {
    float x, y, z, scale;    // 16 bytes
    float qx, qy, qz, qw;    // 16 bytes (quaternion)
};

// Boids simulation parameters
struct BoidsParams {
    float separationRadius;
    float alignmentRadius;
    float cohesionRadius;
    
    float separationWeight;
    float alignmentWeight;
    float cohesionWeight;
    
    float maxSpeed;
    float minSpeed;
    float maxForce;
    
    float boundaryMargin;
    float boundaryForce;
    float noiseStrength;
    float deltaTime;
    
    float3 boundsMin;
    float3 boundsMax;
};

// Agent data in Structure of Arrays for coalesced access
struct AgentArrays {
    float* pos_x;
    float* pos_y;
    float* pos_z;
    float* vel_x;
    float* vel_y;
    float* vel_z;
    int count;
};

// Simulation state
struct SimulationState {
    AgentArrays arrays;
    BoidsParams params;
    cudaGraphicsResource_t cudaVBO;
    bool vboRegistered;
};

// CUDA error checking
#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA error at %s:%d: %s\n", __FILE__, __LINE__, \
                    cudaGetErrorString(err)); \
        } \
    } while(0)

// Device helper functions
__device__ float3 operator+(const float3& a, const float3& b) {
    return make_float3(a.x + b.x, a.y + b.y, a.z + b.z);
}

__device__ float3 operator-(const float3& a, const float3& b) {
    return make_float3(a.x - b.x, a.y - b.y, a.z - b.z);
}

__device__ float3 operator*(const float3& a, float s) {
    return make_float3(a.x * s, a.y * s, a.z * s);
}

__device__ float3 operator/(const float3& a, float s) {
    return make_float3(a.x / s, a.y / s, a.z / s);
}

__device__ float length(const float3& v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

__device__ float3 normalize(const float3& v) {
    float len = length(v);
    return len > 0.0001f ? v / len : make_float3(0, 0, 0);
}

// Convert velocity to quaternion (fish looks along velocity)
__device__ void velocityToQuaternion(const float3& vel, float& qx, float& qy, float& qz, float& qw) {
    float3 forward = normalize(vel);
    if (length(forward) < 0.001f) {
        forward = make_float3(1, 0, 0);
    }
    
    float3 worldUp = make_float3(0, 0, 1);
    float3 right = normalize(make_float3(
        worldUp.y * forward.z - worldUp.z * forward.y,
        worldUp.z * forward.x - worldUp.x * forward.z,
        worldUp.x * forward.y - worldUp.y * forward.x
    ));
    
    if (length(right) < 0.001f) {
        worldUp = make_float3(0, 1, 0);
        right = normalize(make_float3(
            worldUp.y * forward.z - worldUp.z * forward.y,
            worldUp.z * forward.x - worldUp.x * forward.z,
            worldUp.x * forward.y - worldUp.y * forward.x
        ));
    }
    
    float3 up = make_float3(
        forward.y * right.z - forward.z * right.y,
        forward.z * right.x - forward.x * right.z,
        forward.x * right.y - forward.y * right.x
    );
    
    // Matrix to quaternion
    float trace = right.x + up.y + forward.z;
    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f) * 2.0f;
        qw = 0.25f * s;
        qx = (up.z - forward.y) / s;
        qy = (forward.x - right.z) / s;
        qz = (right.y - up.x) / s;
    } else if (right.x > up.y && right.x > forward.z) {
        float s = sqrtf(1.0f + right.x - up.y - forward.z) * 2.0f;
        qw = (up.z - forward.y) / s;
        qx = 0.25f * s;
        qy = (up.x + right.y) / s;
        qz = (forward.x + right.z) / s;
    } else if (up.y > forward.z) {
        float s = sqrtf(1.0f + up.y - right.x - forward.z) * 2.0f;
        qw = (forward.x - right.z) / s;
        qx = (up.x + right.y) / s;
        qy = 0.25f * s;
        qz = (forward.y + up.z) / s;
    } else {
        float s = sqrtf(1.0f + forward.z - right.x - up.y) * 2.0f;
        qw = (right.y - up.x) / s;
        qx = (forward.x + right.z) / s;
        qy = (forward.y + up.z) / s;
        qz = 0.25f * s;
    }
}

// Initialize agents kernel
__global__ void initializeAgentsKernel(
    AgentArrays arrays,
    BoidsParams params,
    unsigned int seed)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= arrays.count) return;
    
    curandState state;
    curand_init(seed, idx, 0, &state);
    
    float3 bmin = params.boundsMin;
    float3 bmax = params.boundsMax;
    
    arrays.pos_x[idx] = bmin.x + curand_uniform(&state) * (bmax.x - bmin.x);
    arrays.pos_y[idx] = bmin.y + curand_uniform(&state) * (bmax.y - bmin.y);
    arrays.pos_z[idx] = bmin.z + curand_uniform(&state) * (bmax.z - bmin.z);
    
    float speed = params.minSpeed + curand_uniform(&state) * (params.maxSpeed - params.minSpeed);
    float theta = curand_uniform(&state) * 2.0f * M_PI;
    float phi = curand_uniform(&state) * M_PI;
    
    arrays.vel_x[idx] = speed * sinf(phi) * cosf(theta);
    arrays.vel_y[idx] = speed * sinf(phi) * sinf(theta);
    arrays.vel_z[idx] = speed * cosf(phi);
}

// Update boids kernel
__global__ void updateBoidsKernel(
    AgentArrays arrays,
    BoidsParams params)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= arrays.count) return;
    
    curandState state;
    curand_init(clock64(), idx, 0, &state);
    
    float3 pos = make_float3(arrays.pos_x[idx], arrays.pos_y[idx], arrays.pos_z[idx]);
    float3 vel = make_float3(arrays.vel_x[idx], arrays.vel_y[idx], arrays.vel_z[idx]);
    
    float3 separation = make_float3(0, 0, 0);
    float3 alignment = make_float3(0, 0, 0);
    float3 cohesion = make_float3(0, 0, 0);
    
    int sepCount = 0, alignCount = 0, cohCount = 0;
    
    // Boids rules
    for (int j = 0; j < arrays.count; j++) {
        if (j == idx) continue;
        
        float3 otherPos = make_float3(arrays.pos_x[j], arrays.pos_y[j], arrays.pos_z[j]);
        float3 diff = pos - otherPos;
        float dist = length(diff);
        
        if (dist < params.separationRadius && dist > 0.0001f) {
            separation = separation + normalize(diff) / dist;
            sepCount++;
        }
        
        if (dist < params.alignmentRadius) {
            float3 otherVel = make_float3(arrays.vel_x[j], arrays.vel_y[j], arrays.vel_z[j]);
            alignment = alignment + otherVel;
            alignCount++;
        }
        
        if (dist < params.cohesionRadius) {
            cohesion = cohesion + otherPos;
            cohCount++;
        }
    }
    
    float3 steer = make_float3(0, 0, 0);
    
    if (sepCount > 0) {
        separation = separation / (float)sepCount;
        steer = steer + separation * params.separationWeight;
    }
    
    if (alignCount > 0) {
        alignment = alignment / (float)alignCount;
        alignment = normalize(alignment) * params.maxSpeed;
        float3 alignSteer = alignment - vel;
        steer = steer + alignSteer * params.alignmentWeight;
    }
    
    if (cohCount > 0) {
        cohesion = cohesion / (float)cohCount;
        float3 desired = cohesion - pos;
        desired = normalize(desired) * params.maxSpeed;
        float3 cohSteer = desired - vel;
        steer = steer + cohSteer * params.cohesionWeight;
    }
    
    // Boundary repulsion
    float3 bmin = params.boundsMin;
    float3 bmax = params.boundsMax;
    
    if (pos.x < bmin.x + params.boundaryMargin) steer.x += params.boundaryForce;
    if (pos.x > bmax.x - params.boundaryMargin) steer.x -= params.boundaryForce;
    if (pos.y < bmin.y + params.boundaryMargin) steer.y += params.boundaryForce;
    if (pos.y > bmax.y - params.boundaryMargin) steer.y -= params.boundaryForce;
    if (pos.z < bmin.z + params.boundaryMargin) steer.z += params.boundaryForce;
    if (pos.z > bmax.z - params.boundaryMargin) steer.z -= params.boundaryForce;
    
    // Add noise
    steer.x += (curand_uniform(&state) - 0.5f) * params.noiseStrength;
    steer.y += (curand_uniform(&state) - 0.5f) * params.noiseStrength;
    steer.z += (curand_uniform(&state) - 0.5f) * params.noiseStrength;
    
    // Limit force
    float steerLen = length(steer);
    if (steerLen > params.maxForce) {
        steer = steer / steerLen * params.maxForce;
    }
    
    // Update velocity
    vel = vel + steer * params.deltaTime;
    
    // Limit speed
    float speed = length(vel);
    if (speed > params.maxSpeed) vel = vel / speed * params.maxSpeed;
    if (speed < params.minSpeed && speed > 0.0001f) vel = vel / speed * params.minSpeed;
    
    // Update position
    pos = pos + vel * params.deltaTime;
    
    // Store back
    arrays.pos_x[idx] = pos.x;
    arrays.pos_y[idx] = pos.y;
    arrays.pos_z[idx] = pos.z;
    arrays.vel_x[idx] = vel.x;
    arrays.vel_y[idx] = vel.y;
    arrays.vel_z[idx] = vel.z;
}

// Write instance transforms to OpenGL VBO
__global__ void writeInstanceTransformsKernel(
    Instance* instances,
    AgentArrays arrays,
    int count)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;
    
    curandState state;
    curand_init(clock64() + idx, idx, 0, &state);
    
    instances[idx].x = arrays.pos_x[idx];
    instances[idx].y = arrays.pos_y[idx];
    instances[idx].z = arrays.pos_z[idx];
    
    // Scale fish based on speed and add variation
    float3 vel = make_float3(arrays.vel_x[idx], arrays.vel_y[idx], arrays.vel_z[idx]);
    float speed = length(vel);
    // Smaller, more realistic fish size with variation
    instances[idx].scale = 0.25f + speed * 0.008f + (curand_uniform(&state) * 0.15f);
    
    velocityToQuaternion(vel, instances[idx].qx, instances[idx].qy, instances[idx].qz, instances[idx].qw);
}

// ===== C API =====

extern "C" {

void* createSimulation(int numAgents, float boundsSize) {
    SimulationState* sim = new SimulationState();
    
    // Allocate device memory
    size_t sz = numAgents * sizeof(float);
    CUDA_CHECK(cudaMalloc(&sim->arrays.pos_x, sz));
    CUDA_CHECK(cudaMalloc(&sim->arrays.pos_y, sz));
    CUDA_CHECK(cudaMalloc(&sim->arrays.pos_z, sz));
    CUDA_CHECK(cudaMalloc(&sim->arrays.vel_x, sz));
    CUDA_CHECK(cudaMalloc(&sim->arrays.vel_y, sz));
    CUDA_CHECK(cudaMalloc(&sim->arrays.vel_z, sz));
    sim->arrays.count = numAgents;
    
    // Default parameters
    sim->params.separationRadius = 5.0f;
    sim->params.alignmentRadius = 15.0f;
    sim->params.cohesionRadius = 20.0f;
    sim->params.separationWeight = 1.5f;
    sim->params.alignmentWeight = 1.0f;
    sim->params.cohesionWeight = 1.0f;
    sim->params.maxSpeed = 20.0f;
    sim->params.minSpeed = 10.0f;
    sim->params.maxForce = 5.0f;
    sim->params.boundaryMargin = boundsSize * 0.1f;
    sim->params.boundaryForce = 50.0f;
    sim->params.noiseStrength = 0.5f;
    sim->params.deltaTime = 0.016f;
    sim->params.boundsMin = make_float3(-boundsSize, -boundsSize, -boundsSize);
    sim->params.boundsMax = make_float3(boundsSize, boundsSize, boundsSize);
    
    sim->vboRegistered = false;
    
    // Initialize agents
    int blockSize = 256;
    int numBlocks = (numAgents + blockSize - 1) / blockSize;
    initializeAgentsKernel<<<numBlocks, blockSize>>>(sim->arrays, sim->params, 12345);
    CUDA_CHECK(cudaDeviceSynchronize());
    
    printf("CUDA simulation created: %d agents\n", numAgents);
    return sim;
}

void destroySimulation(void* handle) {
    if (!handle) return;
    SimulationState* sim = (SimulationState*)handle;
    
    if (sim->vboRegistered) {
        cudaGraphicsUnregisterResource(sim->cudaVBO);
    }
    
    cudaFree(sim->arrays.pos_x);
    cudaFree(sim->arrays.pos_y);
    cudaFree(sim->arrays.pos_z);
    cudaFree(sim->arrays.vel_x);
    cudaFree(sim->arrays.vel_y);
    cudaFree(sim->arrays.vel_z);
    
    delete sim;
}

void registerVBO(void* handle, unsigned int vbo) {
    if (!handle) return;
    SimulationState* sim = (SimulationState*)handle;
    
    if (sim->vboRegistered) {
        cudaGraphicsUnregisterResource(sim->cudaVBO);
    }
    
    CUDA_CHECK(cudaGraphicsGLRegisterBuffer(&sim->cudaVBO, vbo, cudaGraphicsMapFlagsWriteDiscard));
    sim->vboRegistered = true;
    printf("VBO registered with CUDA\n");
}

void updateAndWriteInstances(void* handle, float deltaTime) {
    if (!handle) return;
    SimulationState* sim = (SimulationState*)handle;
    
    if (!sim->vboRegistered) {
        printf("VBO not registered!\n");
        return;
    }
    
    sim->params.deltaTime = deltaTime;
    
    int blockSize = 256;
    int numBlocks = (sim->arrays.count + blockSize - 1) / blockSize;
    
    // Update boids
    updateBoidsKernel<<<numBlocks, blockSize>>>(sim->arrays, sim->params);
    CUDA_CHECK(cudaGetLastError());
    
    // Map VBO
    CUDA_CHECK(cudaGraphicsMapResources(1, &sim->cudaVBO, 0));
    
    Instance* devPtr;
    size_t size;
    CUDA_CHECK(cudaGraphicsResourceGetMappedPointer((void**)&devPtr, &size, sim->cudaVBO));
    
    // Write transforms
    writeInstanceTransformsKernel<<<numBlocks, blockSize>>>(devPtr, sim->arrays, sim->arrays.count);
    CUDA_CHECK(cudaGetLastError());
    
    // Unmap VBO
    CUDA_CHECK(cudaGraphicsUnmapResources(1, &sim->cudaVBO, 0));
}

void setParameters(void* handle, float separation, float alignment, float cohesion) {
    if (!handle) return;
    SimulationState* sim = (SimulationState*)handle;
    sim->params.separationWeight = separation;
    sim->params.alignmentWeight = alignment;
    sim->params.cohesionWeight = cohesion;
}

} // extern "C"
