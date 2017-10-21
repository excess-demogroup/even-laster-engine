class Scene;
class Mesh;
class Transform;

struct aiScene;
struct aiMesh;
struct aiNode;

#include <vector>

class SceneImporter
{
public:
	static Scene *import(std::string filename);

private:
	SceneImporter(const aiScene *source);

	std::vector<Mesh*> meshes;

	Mesh *convertMesh(aiMesh *mesh);
	void convertMeshes();

	void traverseChildren(const aiNode *node, Transform *parentTransform);
	void traverseNode(const aiNode *node, Transform *parentTransform);

	const aiScene *source;
	Scene *result;
};
