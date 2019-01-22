#include "model.h"

#include <assimp/DefaultLogger.hpp>

namespace filesystem = std::experimental::filesystem;

namespace DirectX
{
  DirectX::XMFLOAT3 XMQuaternionToEuler(DirectX::XMVECTOR quaternion_vec)
  {
    DirectX::XMFLOAT3 output;
    float sqw;
    float sqx;
    float sqy;
    float sqz;

    DirectX::XMFLOAT4 quaternion;
    DirectX::XMStoreFloat4(&quaternion, quaternion_vec);

    sqw = quaternion.w * quaternion.w;
    sqx = quaternion.x * quaternion.x;
    sqy = quaternion.y * quaternion.y;
    sqz = quaternion.z * quaternion.z;

    float unit = sqw + sqx + sqy + sqz;
    float test = quaternion.x * quaternion.w - quaternion.y * quaternion.z;

    if (test > 0.4995f * unit)
    {
      output.y = (2.0f * atan2(quaternion.y, quaternion.x));
      output.x = (DirectX::XM_PIDIV2);

      return output;
    }
    if (test < -0.4995f * unit)
    {
      output.y = (-2.0f * atan2(quaternion.y, quaternion.x));
      output.x = (-DirectX::XM_PIDIV2);

      return output;
    }

    output.x = (asin(2.0f * (quaternion.w * quaternion.x - quaternion.z * quaternion.y)));
    output.y = (atan2(2.0f * quaternion.w * quaternion.y + 2.0f * quaternion.z * quaternion.x, 1 - 2.0f * (sqx + sqy)));
    output.z = (atan2(2.0f * quaternion.w * quaternion.z + 2.0f * quaternion.x * quaternion.y, 1 - 2.0f * (sqz + sqx)));

    return output;
  }
}

namespace rtrt
{
  //------------------------------------------------------------------------------------------------------
  Model::Model() :
    scene_(nullptr),
    root_node(nullptr)
  {
    Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE, aiDefaultLogStream_STDOUT, nullptr);
  }

  //------------------------------------------------------------------------------------------------------
  Model::~Model()
  {
    std::function<void(Node*)> DeleteNode = [&](Node* node)->void
    {
      for (size_t i = 0; i < node->children.size(); i++)
      {
        DeleteNode(node->children[i]);
      }

      delete node;
    };

    if (root_node != nullptr)
    {
      DeleteNode(root_node);
    }
  }

  //------------------------------------------------------------------------------------------------------
  void Model::LoadFromFile(const std::string& imodel_file_path)
  {
    filesystem::path model_file_path_fs = imodel_file_path;
    std::wstring model_directory_path_wchar = model_file_path_fs.parent_path().c_str();

    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;

    model_file_path_ = imodel_file_path;
    model_directory_path_ = converter.to_bytes(model_directory_path_wchar);

    scene_ = importer_.ReadFile(imodel_file_path.c_str(), 
      aiProcess_GenNormals |
      aiProcess_CalcTangentSpace |
      aiProcess_Triangulate |
      aiProcess_FlipUVs
    );
    
    root_node = ProcessNode(scene_->mRootNode, nullptr);
    ProcessMeshes(scene_->mMeshes, scene_->mNumMeshes);
    ProcessMaterials(scene_->mMaterials, scene_->mNumMaterials);
  }

  //------------------------------------------------------------------------------------------------------
  Model::Node* Model::ProcessNode(aiNode* inode, Model::Node* parent)
  {
    Node* node = new Node();

    node->parent = parent;

    node->transform.r[0] = DirectX::XMVectorSet(inode->mTransformation.a1, inode->mTransformation.b1, inode->mTransformation.c1, inode->mTransformation.d1);
    node->transform.r[1] = DirectX::XMVectorSet(inode->mTransformation.a2, inode->mTransformation.b2, inode->mTransformation.c2, inode->mTransformation.d2);
    node->transform.r[2] = DirectX::XMVectorSet(inode->mTransformation.a3, inode->mTransformation.b3, inode->mTransformation.c3, inode->mTransformation.d3);
    node->transform.r[3] = DirectX::XMVectorSet(inode->mTransformation.a4, inode->mTransformation.b4, inode->mTransformation.c4, inode->mTransformation.d4);

    DirectX::XMVECTOR scaling_vec, quaternion_vec, position_vec;
    ThrowIfFalse(DirectX::XMMatrixDecompose(&scaling_vec, &quaternion_vec, &position_vec, node->transform) == true);

    DirectX::XMStoreFloat3(&node->scale, scaling_vec);
    DirectX::XMStoreFloat3(&node->position, position_vec);
    node->rotation = DirectX::XMQuaternionToEuler(quaternion_vec);

    for (UINT i = 0; i < inode->mNumMeshes; i++)
    {
      node->meshes.push_back(inode->mMeshes[i]);
    }

    for (UINT i = 0; i < inode->mNumChildren; i++)
    {
      node->children.push_back(ProcessNode(inode->mChildren[i], node));
    }

    return node;
  }
  
  //------------------------------------------------------------------------------------------------------
  void Model::ProcessMeshes(aiMesh** imeshes, UINT num_meshes)
  {
    for (unsigned int i = 0; i < num_meshes; i++)
    {
      Mesh mesh;

      for(unsigned int j = 0; j < imeshes[i]->mNumVertices; j++)
      {
        Vertex vert;

        vert.position = DirectX::XMFLOAT3(
          imeshes[i]->mVertices[j].x,
          imeshes[i]->mVertices[j].y,
          imeshes[i]->mVertices[j].z
        );

        if (imeshes[i]->HasNormals())
        {
          vert.normal = DirectX::XMFLOAT3(
            imeshes[i]->mNormals[j].x,
            imeshes[i]->mNormals[j].y,
            imeshes[i]->mNormals[j].z
          );
        }

        if (imeshes[i]->HasTangentsAndBitangents())
        {
          vert.tangent = DirectX::XMFLOAT3(
            imeshes[i]->mTangents[j].x,
            imeshes[i]->mTangents[j].y,
            imeshes[i]->mTangents[j].z
          );
        }

        if (imeshes[i]->HasTextureCoords(0))
        {
          vert.uv = DirectX::XMFLOAT2(
            imeshes[i]->mTextureCoords[0][j].x,
            imeshes[i]->mTextureCoords[0][j].y
          );
        }

        if (imeshes[i]->HasVertexColors(0))
        {
          vert.color = DirectX::XMFLOAT4(
            imeshes[i]->mColors[0][j].r,
            imeshes[i]->mColors[0][j].g,
            imeshes[i]->mColors[0][j].b,
            imeshes[i]->mColors[0][j].a
          );
        }

        mesh.vertices.push_back(vert);
      }

      for (unsigned int j = 0; j < imeshes[i]->mNumFaces; j++)
      {
        aiFace& face = imeshes[i]->mFaces[j];

        for (unsigned int k = 0; k < face.mNumIndices; k++)
        {
          mesh.indices.push_back(face.mIndices[k]);
        }
      }

      mesh.material = imeshes[i]->mMaterialIndex;
      mesh.name = imeshes[i]->mName.C_Str();

      meshes.push_back(mesh);
    }
  }

  //------------------------------------------------------------------------------------------------------
  void Model::ProcessMaterials(aiMaterial** imaterials, UINT num_materials)
  {
    std::unordered_map<std::string, UINT> texture_indices;

    for (unsigned int i = 0; i < num_materials; i++)
    {
      const aiMaterial* current = imaterials[i];
      aiString material_name;
      aiColor3D color_diffuse, color_specular, color_ambient, color_emissive;
      float opacity = 1.0f, specular_scale = 0.5f, specular_power = 1.0f, bump_intensity = 5.0f, index_of_refraction = 1.0f;
      int shading_model;

      current->Get(AI_MATKEY_NAME, material_name);
      current->Get(AI_MATKEY_COLOR_DIFFUSE, color_diffuse);
      current->Get(AI_MATKEY_COLOR_SPECULAR, color_specular);
      current->Get(AI_MATKEY_COLOR_AMBIENT, color_ambient);
      current->Get(AI_MATKEY_COLOR_EMISSIVE, color_emissive);
      current->Get(AI_MATKEY_OPACITY, opacity);
      current->Get(AI_MATKEY_SHININESS, specular_scale);
      current->Get(AI_MATKEY_SHININESS_STRENGTH, specular_power);
      current->Get(AI_MATKEY_BUMPSCALING, bump_intensity);
      current->Get(AI_MATKEY_SHADING_MODEL, shading_model);
      current->Get(AI_MATKEY_REFRACTI, index_of_refraction);

      Material material;
      material.name = material_name.data;
      material.color_diffuse = DirectX::XMFLOAT4(color_diffuse.r, color_diffuse.g, color_diffuse.b, 1.0f);
      material.color_specular = DirectX::XMFLOAT4(color_specular.r, color_specular.g, color_specular.b, 1.0f);
      material.color_ambient = DirectX::XMFLOAT4(color_ambient.r, color_ambient.g, color_ambient.b, 1.0f);
      material.color_emissive = DirectX::XMFLOAT4(color_emissive.r, color_emissive.g, color_emissive.b, 1.0f);
      material.opacity = opacity;
      material.specular_scale = specular_scale;
      material.specular_power = std::min(std::max(specular_power, 2.0f), 1000.0f);
      material.bump_intensity = bump_intensity;
      material.index_of_refraction = index_of_refraction;
      material.shading_model = shading_model;
      material.ambient_map = MATERIAL_NO_TEXTURE_INDEX;
      material.emissive_map = MATERIAL_NO_TEXTURE_INDEX;
      material.normal_map = MATERIAL_NO_TEXTURE_INDEX;
      material.bump_map = MATERIAL_NO_TEXTURE_INDEX;
      material.specular_map = MATERIAL_NO_TEXTURE_INDEX;
      material.specular_power_map = MATERIAL_NO_TEXTURE_INDEX;
      material.diffuse_map = MATERIAL_NO_TEXTURE_INDEX;
      material.glossiness = 0.0f;

      for (int i = 0; i < aiTextureType_UNKNOWN; i++)
      {
        if (IsTextureTypeSupported(static_cast<aiTextureType>(i)))
        {
          int texture_count = current->GetTextureCount(static_cast<aiTextureType>(i));

          if (texture_count > 0)
          {
            aiString path;
            current->GetTexture(static_cast<aiTextureType>(i), 0, &path);

            std::string full_path = model_directory_path_ + "/" + path.data;

            UINT texture_id;

            if (texture_indices.find(full_path) == texture_indices.end())
            {
              Texture texture;
              texture.path = full_path.c_str();

              texture_indices.insert(std::make_pair(full_path, (UINT)textures.size()));
              texture_id = (UINT)textures.size();
              textures.push_back(texture);
            }
            else
            {
              texture_id = texture_indices[full_path];
            }

            switch (static_cast<aiTextureType>(i))
            {
            case aiTextureType_AMBIENT:
              material.ambient_map = texture_id;
              break;
            case aiTextureType_DIFFUSE:
              material.diffuse_map = texture_id;
              break;
            case aiTextureType_EMISSIVE:
              material.emissive_map = texture_id;
              break;
            case aiTextureType_HEIGHT:
              material.bump_map = texture_id;
              break;
            case aiTextureType_NORMALS:
              material.normal_map = texture_id;
              break;
            case aiTextureType_SHININESS:
              material.specular_power_map = texture_id;
              break;
            case aiTextureType_SPECULAR:
              material.specular_map = texture_id;
              break;
            }
          }
        }
      }

      materials.push_back(material);
    }
  }
  
  //------------------------------------------------------------------------------------------------------
  bool Model::IsTextureTypeSupported(aiTextureType type)
  {
    switch (type)
    {
    case aiTextureType_AMBIENT: return true;
    case aiTextureType_DIFFUSE: return true;
    case aiTextureType_EMISSIVE: return true;
    case aiTextureType_HEIGHT: return true;
    case aiTextureType_NORMALS: return true;
    case aiTextureType_SHININESS: return true;
    case aiTextureType_SPECULAR: return true;
    }

    return false;
  }
}