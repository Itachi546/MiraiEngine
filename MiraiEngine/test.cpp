#include <iostream>
#include "ThirdParty/glm/glm/glm.hpp"
int main() {
    glm::vec2 v = glm::vec2{0.5f};
    std::cout << v.x << " " << v.y << std::endl;
    return 0;
}
