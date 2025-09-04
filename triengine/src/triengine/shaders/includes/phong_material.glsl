struct PhongShadedObjectMaterial
{
    float ambientIntensity; // ambient term intensity
    float diffuseIntensity; // diffuse term intensity
    float specularIntensity; // specular term intensity
    float shininess; // object surface shininess scalar (must be `> 0`)
};