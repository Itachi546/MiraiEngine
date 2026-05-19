#version 460

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

layout(binding = 0) readonly buffer SourceDataBuffer {
    uint src[];
};

struct BufferPatch {
    // In 4 bytes
    uint offset;
    // In 4 bytes
    uint size;
};

layout(push_constant) uniform PushConstant {
    uint total_patches;
    uint _padding[3];
};

layout(binding = 1) readonly buffer PatchInfoBuffer {
    BufferPatch patches[];
};

layout(binding = 2) buffer DestinationBuffer {
    uint dst[];
};

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id > total_patches)
        return;

    BufferPatch current_patch = patches[id];

    uint src_offset = id * current_patch.size;
    for (int i = 0; i < current_patch.size; ++i) {
        dst[current_patch.offset + i] = src[src_offset + i];
    }
}