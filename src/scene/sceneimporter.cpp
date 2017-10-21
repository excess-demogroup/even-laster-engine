#include "sceneimporter.h"
#include "scene.h"

#include "../core/blobbuilder.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <stdexcept>

using std::string;
using std::runtime_error;

Scene *SceneImporter::import(string filename)
{
	Assimp::Importer importer;

	importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, 0x0000FFFF); // max 16-bit indices
	importer.SetPropertyInteger(AI_CONFIG_PP_SLM_TRIANGLE_LIMIT, 0x00FFFFFF);
	importer.SetPropertyInteger(AI_CONFIG_PP_FD_REMOVE, 1);
	importer.SetPropertyInteger(AI_CONFIG_PP_PTV_KEEP_HIERARCHY, 0);

	auto flags = aiProcessPreset_TargetRealtime_MaxQuality | aiProcess_ConvertToLeftHanded;
	auto source = importer.ReadFile(filename, flags);
	if (!source)
		throw runtime_error(importer.GetErrorString());

	auto sceneImporter = SceneImporter(source);
	sceneImporter.convertMeshes();
	sceneImporter.traverseChildren(source->mRootNode, nullptr);

	return sceneImporter.result;
}

SceneImporter::SceneImporter(const aiScene *source) :
	source(source),
	result(new Scene())
{
}

Mesh *SceneImporter::convertMesh(aiMesh *mesh)
{
	VertexFormat vertexFormat = VERTEX_FORMAT_NONE;
	int stride = 0;
	if (mesh->HasPositions()) {
		vertexFormat |= VERTEX_FORMAT_POSITION;
		stride += sizeof(float) * 3;
	}
	if (mesh->HasNormals()) {
		vertexFormat |= VERTEX_FORMAT_NORMAL;
		stride += sizeof(float) * 3;
	}
	if (mesh->HasTangentsAndBitangents()) {
		vertexFormat |= VERTEX_FORMAT_TANGENT | VERTEX_FORMAT_BINORMAL;
		stride += sizeof(float) * 6;
	}

	BlobBuilder vertexBuffer;
	for (auto i = 0u; i < mesh->mNumVertices; ++i) {
		if (mesh->HasPositions()) {
			vertexBuffer.append(mesh->mVertices[i].x);
			vertexBuffer.append(mesh->mVertices[i].y);
			vertexBuffer.append(mesh->mVertices[i].z);
		}

		if (mesh->HasNormals()) {
			vertexBuffer.append(mesh->mNormals[i].x);
			vertexBuffer.append(mesh->mNormals[i].y);
			vertexBuffer.append(mesh->mNormals[i].z);
		}

		if (mesh->HasTangentsAndBitangents()) {
			vertexBuffer.append(mesh->mTangents[i].x);
			vertexBuffer.append(mesh->mTangents[i].y);
			vertexBuffer.append(mesh->mTangents[i].z);
			vertexBuffer.append(mesh->mBitangents[i].x);
			vertexBuffer.append(mesh->mBitangents[i].y);
			vertexBuffer.append(mesh->mBitangents[i].z);
		}
	}
	auto vertexData = vertexBuffer.getBytes();

	auto indexType = INDEX_TYPE_UINT32;
	BlobBuilder indexBuffer;
	for (auto i = 0u; i < mesh->mNumFaces; ++i) {
		aiFace *face = mesh->mFaces + i;
		assert(face->mNumIndices == mesh->mFaces[0].mNumIndices);

		for (auto j = 0u; j < face->mNumIndices; ++j)
			indexBuffer.append(face->mIndices[j]);
	}
	auto indexData = indexBuffer.getBytes();

	return new Mesh(vertexData, vertexFormat, indexData, indexType);
}

void SceneImporter::convertMeshes()
{
	meshes.resize(source->mNumMeshes);
	for (auto i = 0u; i < source->mNumMeshes; ++i)
		meshes[i] = convertMesh(source->mMeshes[i]);
}

void SceneImporter::traverseChildren(const aiNode *node, Transform *parentTransform)
{
	for (auto i = 0u; i < node->mNumChildren; ++i)
		traverseNode(node->mChildren[i], parentTransform);
}

void SceneImporter::traverseNode(const aiNode *node, Transform *parentTransform)
{
	if (!node->mTransformation.IsIdentity()) {
		aiQuaternion rot;
		aiVector3D scale, trans;
		node->mTransformation.Decompose(scale, rot, trans);

		auto transform = result->createMatrixTransform(parentTransform);

		auto rotation = glm::toMat4(glm::quat(rot.w, rot.x, rot.y, rot.z));
		auto scaling = glm::scale(glm::mat4(1), glm::vec3(scale.x, scale.y, scale.z));
		auto translate = glm::translate(glm::mat4(1), glm::vec3(trans.x, trans.y, trans.z));
		auto localMatrix = scaling * rotation * translate;
//		auto localMatrix = translate * rotation * scaling;

		transform->setLocalMatrix(localMatrix);
		parentTransform = transform;
	}

	for (auto i = 0u; i < node->mNumMeshes; ++i) {
		auto mesh = meshes[node->mMeshes[i]];
//		auto material = source->mMaterials[mesh->mMaterialIndex];
		auto material = new Material();
		auto model = new Model(mesh, material);
		auto object = result->createObject(model, parentTransform);

		/*
		auto mesh = Mesh(vertices, indices);
		auto material = Material();

		auto object = result->createObject(model, parentTransform);
		*/

		/*
		snprintf(buf, sizeof(buf), "%s-%d.mesh", node->mName.data, i);
		for (int j = 0; buf[j]; ++j)
		if (!allowed_path_char(buf[j]))
		buf[j] = '_';

		ret = dump_mesh(buf, mesh);
		if (ret)
		return -1;

		TiXmlElement *objectElem = new TiXmlElement("object");
		objectElem->SetAttribute("id", node->mName.data);
		TiXmlElement *transformElem = new TiXmlElement("transform");
		transformElem->SetAttribute("ref", transform_name);
		objectElem->LinkEndChild(transformElem);
		TiXmlElement *meshElem = new TiXmlElement("mesh");
		meshElem->SetAttribute("src", buf);
		objectElem->LinkEndChild(meshElem);

		dump_material(ais->mMaterials[mesh->mMaterialIndex], objectElem);
		objectsElem->LinkEndChild(objectElem);
		*/
	}

	traverseChildren(node, parentTransform);
}
