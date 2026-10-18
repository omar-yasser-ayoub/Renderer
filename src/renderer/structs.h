#include <glm/glm.hpp>

struct MaterialUBO {
    glm::vec4 baseColor;
    float metallicFactor;
    float roughnessFactor;
    float _pad[2];
};

struct CameraUBO {
    glm::mat4 viewProjMatrix;
    glm::vec4 position;
};

struct ModelUBO {
    glm::mat4 modelMatrix;
};
