#pragma once

#include <string>
#include <vector>

#include "shared/raytracing_data.h"

namespace rtrt
{
  using Index = uint32_t;

  class Model
  {
  public:
    struct Node
    {
      DirectX::XMMATRIX transform;
      DirectX::XMFLOAT3 position;
      DirectX::XMFLOAT3 rotation;
      DirectX::XMFLOAT3 scale;
      std::string name;
      std::vector<UINT> meshes;
      std::vector<Node*> children;
      Node* parent;
    };

    struct Mesh
    {
      std::string name;
      std::vector<Vertex> vertices;
      std::vector<Index> indices;
      UINT material;
    };

    struct Texture
    {
      std::string path;
    };

    struct Material
    {
      std::string name;
      DirectX::XMFLOAT4 color_emissive;
      DirectX::XMFLOAT4 color_ambient;
      DirectX::XMFLOAT4 color_diffuse;
      DirectX::XMFLOAT4 color_specular;
      float opacity;
      float specular_scale;
      float specular_power;
      float bump_intensity;
      UINT emissive_map;
      UINT ambient_map;
      UINT diffuse_map;
      UINT specular_map;
      UINT specular_power_map;
      UINT bump_map;
      UINT normal_map;
      float index_of_refraction;
      UINT shading_model;
    };

    Model();
    ~Model();

    void LoadFromFile(const std::string& model_file_path);

    Node* root_node;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;

  protected:
    Node* ProcessNode(aiNode* node, Node* parent);
    void ProcessMeshes(aiMesh** meshes, UINT num_meshes);
    void ProcessMaterials(aiMaterial** materials, UINT num_materials);

    bool IsTextureTypeSupported(aiTextureType type);
  private:
    std::string model_file_path_;
    std::string model_directory_path_;
    Assimp::Importer importer_;
    const aiScene* scene_;
  };
}