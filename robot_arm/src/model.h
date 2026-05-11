#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "mesh.h"
#include "shader.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
using namespace std;



class Model 
{
public:
    /*  Model Data */
    Mesh mesh;
    Texture* diffuse;
    Texture* normal;
    Texture* specular;
    unsigned int VAO;
    bool ignoreShadow = false;

    Model() {}
    /*  Functions   */
    // constructor, expects a filepath to a 3D model.
    Model(string const &path):diffuse(NULL), normal(NULL), specular(NULL){
        loadModel(path);
        this->VAO = mesh.VAO;
    }

    static unsigned int getFallbackWhiteTextureID() {
        static unsigned int fallbackTextureID = 0;
        if (fallbackTextureID != 0) {
            return fallbackTextureID;
        }

        unsigned char whitePixel[] = { 255, 255, 255, 255 };
        glGenTextures(1, &fallbackTextureID);
        glBindTexture(GL_TEXTURE_2D, fallbackTextureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        return fallbackTextureID;
    }

    void bind() {
        glBindVertexArray(mesh.VAO);
        glActiveTexture(GL_TEXTURE0);
        unsigned int diffuseID = this->diffuse ? this->diffuse->ID : getFallbackWhiteTextureID();
        glBindTexture(GL_TEXTURE_2D, diffuseID);
        if (this->specular) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, specular->ID);
        }
        if (this->normal) {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, normal->ID);
        }
    }

    void Draw(Shader &shader) {
        bind();
        glDrawElements(GL_TRIANGLES, mesh.indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
    
private:
    /*  Functions   */
    // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string const &path)
    {
        // read file via ASSIMP
        Assimp::Importer importer;

        // TODO : to get additional 3 points, DO NOT use aiProcess_CalcTangentSpace!
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_PreTransformVertices);
        // check for errors
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }

        vector<Vertex> vertices;
        vector<unsigned int> indices;

        // Collect all meshes in the hierarchy so split DAE meshes are rendered as one model.
        processNode(scene->mRootNode, scene, vertices, indices);

        if (vertices.empty() || indices.empty()) {
            cout << "ERROR::ASSIMP:: no mesh found in model: " << path << endl;
            return;
        }

        this->mesh = Mesh(vertices, indices);
    }

    // Original code : processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    // modified version : only process FIRST item of obj file. Do not consider texture or material.
    void processNode(aiNode *node, const aiScene *scene, vector<Vertex>& outVertices, vector<unsigned int>& outIndices)
    {
        if (!node || !scene) {
            return;
        }

        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            processMesh(mesh, outVertices, outIndices);
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            processNode(node->mChildren[i], scene, outVertices, outIndices);
        }
    }

    void processMesh(aiMesh *mesh, vector<Vertex>& outVertices, vector<unsigned int>& outIndices)
    {
        if (!mesh) {
            return;
        }

        const unsigned int baseIndex = static_cast<unsigned int>(outVertices.size());

        // Walk through each of the mesh's vertices
        for(unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
            // positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            // normals
            if (mesh->HasNormals()) {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
            } else {
                vector = glm::vec3(0.0f, 1.0f, 0.0f);
            }
            vertex.Normal = vector;
            // texture coordinates
            if(mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't 
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x; 
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            }
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            // tangent

            // TODO : to get additional 3 points, DO NOT use mTangents directly!
            if (mesh->mTangents) {
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
            } else {
                vector = glm::vec3(1.0f, 0.0f, 0.0f);
            }
            vertex.Tangent = vector;
            outVertices.push_back(vertex);
        }
        // now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                outIndices.push_back(baseIndex + face.mIndices[j]);
        }
    }
};
class Entity {
public:
    Model* model;
    glm::mat4 modelMatrix;
    Entity(Model* model, glm::mat4 modelMatrix) {
        this->model = model;
        this->modelMatrix = modelMatrix;
    }

    Entity(Model* model, glm::vec3 position, float rotX, float rotY, float rotZ, float scale) {
        this->model = model;
        glm::mat4 transform = glm::mat4(1.0f);
        transform = glm::translate(transform, position);
        transform = glm::rotate(transform, glm::radians(rotX), glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(rotZ), glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, glm::vec3(scale));

        this->modelMatrix = transform;
    }

    glm::mat4 getModelMatrix() {
        return this->modelMatrix;
    }
};

#endif
