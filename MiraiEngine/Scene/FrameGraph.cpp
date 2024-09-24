#include "FrameGraph.hpp"

namespace mirai
{
    // @TODO Create from JSON file as well
    FrameGraph::FrameGraph(FrameGraphBuilder *builder) : builder(builder)
    {
    }

    void FrameGraph::compile()
    {
        // @TODO
    }

    void FrameGraph::render(CommandBuffer *command_buffer, Scene *scene)
    {
    }

} // namespace mirai