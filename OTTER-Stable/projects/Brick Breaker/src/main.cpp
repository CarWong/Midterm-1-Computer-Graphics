#include <Logging.h>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <json.hpp>
#include <fstream>
#include <sstream>

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)

// Graphics
#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture2D.h"
#include "Graphics/VertexTypes.h"

// Utilities
#include "Utils/MeshBuilder.h"
#include "Utils/MeshFactory.h"
#include "Utils/ObjLoader.h"
#include "Utils/ImGuiHelper.h"

#include "Camera.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/FileHelpers.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/StringUtils.h"

//#define LOG_GL_NOTIFICATIONS

/*
	Handles debug messages from OpenGL
	https://www.khronos.org/opengl/wiki/Debug_Output#Message_Components
	@param source    Which part of OpenGL dispatched the message
	@param type      The type of message (ex: error, performance issues, deprecated behavior)
	@param id        The ID of the error or message (to distinguish between different types of errors, like nullref or index out of range)
	@param severity  The severity of the message (from High to Notification)
	@param length    The length of the message
	@param message   The human readable message from OpenGL
	@param userParam The pointer we set with glDebugMessageCallback (should be the game pointer)
*/
void GlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	std::string sourceTxt;
	switch (source) {
	case GL_DEBUG_SOURCE_API: sourceTxt = "DEBUG"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceTxt = "WINDOW"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceTxt = "SHADER"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY: sourceTxt = "THIRD PARTY"; break;
	case GL_DEBUG_SOURCE_APPLICATION: sourceTxt = "APP"; break;
	case GL_DEBUG_SOURCE_OTHER: default: sourceTxt = "OTHER"; break;
	}
	switch (severity) {
	case GL_DEBUG_SEVERITY_LOW:          LOG_INFO("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       LOG_WARN("[{}] {}", sourceTxt, message); break;
	case GL_DEBUG_SEVERITY_HIGH:         LOG_ERROR("[{}] {}", sourceTxt, message); break;
#ifdef LOG_GL_NOTIFICATIONS
	case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO("[{}] {}", sourceTxt, message); break;
#endif
	default: break;
	}
}

// Stores our GLFW window in a global variable for now
GLFWwindow* window;
// The current size of our window in pixels
glm::ivec2 windowSize = glm::ivec2(850, 850);
// The title of our GLFW window
std::string windowTitle = "ICG Midterm | Brick Breaker | Anthony Brown, Kyra Trinidad, and Carolyn Wong";

void GlfwWindowResizedCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	windowSize = glm::ivec2(width, height);
}

//Variables
bool win = false; //check if player won
bool lose = false; //check if player lost
GLfloat dirX, dirY = 1.0f, speedX = 0.0f, speedY = 0.013f, radius = 0.3f; //properties of ball
GLfloat brickRadius = 0.63f; //properties of brick
int brickCount = 0; //when hits 5, player wins

//key input to move paddle
GLfloat movePaddle(GLfloat xPos) {
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { //move left
		if (xPos > -6.12)
			xPos -= 0.05;
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { //move right
		if (xPos < 6.12)
			xPos += 0.05;
	}

	return xPos;
}

GLfloat calcSpeed(GLfloat speedX, GLfloat speedY) {
	return (sqrt((pow(speedX, 2)) + (pow(speedY, 2))));
}

GLfloat calcDist(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) {
	return (sqrt((pow((x2 - x1), 2) + pow((y2 - y1), 2))));
}


/// <summary>
/// Handles intializing GLFW, should be called before initGLAD, but after Logger::Init()
/// Also handles creating the GLFW window
/// </summary>
/// <returns>True if GLFW was initialized, false if otherwise</returns>
bool initGLFW() {
	// Initialize GLFW
	if (glfwInit() == GLFW_FALSE) {
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

	//Create a new GLFW window and make it current
	window = glfwCreateWindow(windowSize.x, windowSize.y, windowTitle.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(window);

	// Set our window resized callback
	glfwSetWindowSizeCallback(window, GlfwWindowResizedCallback);

	return true;
}

/// <summary>
/// Handles initializing GLAD and preparing our GLFW window for OpenGL calls
/// </summary>
/// <returns>True if GLAD is loaded, false if there was an error</returns>
bool initGLAD() {
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		LOG_ERROR("Failed to initialize Glad");
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////
////////////////// NEW IN WEEK 7 /////////////////////
//////////////////////////////////////////////////////

glm::mat4 MAT4_IDENTITY = glm::mat4(1.0f);
glm::mat3 MAT3_IDENTITY = glm::mat3(1.0f);

glm::vec4 UNIT_X = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
glm::vec4 UNIT_Y = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
glm::vec4 UNIT_Z = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
glm::vec4 UNIT_W = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
glm::vec4 ZERO = glm::vec4(0.0f);
glm::vec4 ONE = glm::vec4(1.0f);

// Helper structure for material parameters
// to our shader
struct MaterialInfo : IResource {
	typedef std::shared_ptr<MaterialInfo> Sptr;
	// A human readable name for the material
	std::string     Name;
	// The shader that the material is using
	Shader::Sptr    Shader;

	// Material shader parameters
	Texture2D::Sptr Texture;
	float           Shininess;

	/// <summary>
	/// Handles applying this material's state to the OpenGL pipeline
	/// Will bind the shader, update material uniforms, and bind textures
	/// </summary>
	virtual void Apply() {
		// Material properties
		Shader->SetUniform("u_Material.Shininess", Shininess);

		// For textures, we pass the *slot* that the texture sure draw from
		Shader->SetUniform("u_Material.Diffuse", 0);

		// Bind the texture
		if (Texture != nullptr) {
			Texture->Bind(0);
		}
	}

	/// <summary>
	/// Loads a material from a JSON blob
	/// </summary>
	static MaterialInfo::Sptr FromJson(const nlohmann::json& data) {
		MaterialInfo::Sptr result = std::make_shared<MaterialInfo>();
		result->OverrideGUID(Guid(data["guid"]));
		result->Name = data["name"].get<std::string>();
		result->Shader = ResourceManager::GetShader(Guid(data["shader"]));

		// material specific parameters
		result->Texture = ResourceManager::GetTexture(Guid(data["texture"]));
		result->Shininess = data["shininess"].get<float>();
		return result;
	}

	/// <summary>
	/// Converts this material into it's JSON representation for storage
	/// </summary>
	nlohmann::json ToJson() const {
		return {
			{ "guid", GetGUID().str() },
			{ "name", Name },
			{ "shader", Shader ? Shader->GetGUID().str() : "" },
			{ "texture", Texture ? Texture->GetGUID().str() : "" },
			{ "shininess", Shininess },
		};
	}
};

// Helper structure to represent an object 
// with a transform, mesh, and material
struct RenderObject {
	// Human readable name for the object
	std::string             Name;
	// Unique ID for the object
	Guid                    GUID;
	// The object's world transform
	glm::mat4               Transform;
	// The object's mesh
	VertexArrayObject::Sptr Mesh;
	// The object's material
	MaterialInfo::Sptr      Material;

	// If we want to use MeshFactory, we can populate this list
	std::vector<MeshBuilderParam> MeshBuilderParams;

	// Position of the object
	glm::vec3 Position;
	// Rotation of the object in Euler angler
	glm::vec3 Rotation;
	// The scale of the object
	glm::vec3 Scale;

	RenderObject() :
		Name("Unknown"),
		GUID(Guid::New()),
		Transform(MAT4_IDENTITY),
		Mesh(nullptr),
		Material(nullptr),
		MeshBuilderParams(std::vector<MeshBuilderParam>()),
		Position(ZERO),
		Rotation(ZERO),
		Scale(ONE) {}

	// Recalculates the Transform from the parameters (pos, rot, scale)
	void RecalcTransform() {
		Rotation = glm::fmod(Rotation, glm::vec3(360.0f)); // Wrap all our angles into the 0-360 degree range
		Transform = glm::translate(MAT4_IDENTITY, Position) * glm::mat4_cast(glm::quat(glm::radians(Rotation))) * glm::scale(MAT4_IDENTITY, Scale);
	}

	// Regenerates this object's mesh if it is using the MeshFactory
	void GenerateMesh() {
		if (MeshBuilderParams.size() > 0) {
			if (Mesh != nullptr) {
				LOG_WARN("Overriding existing mesh!");
			}
			MeshBuilder<VertexPosNormTexCol> mesh;
			for (int ix = 0; ix < MeshBuilderParams.size(); ix++) {
				MeshFactory::AddParameterized(mesh, MeshBuilderParams[ix]);
			}
			Mesh = mesh.Bake();
		}
	}

	/// <summary>
	/// Loads a render object from a JSON blob
	/// </summary>
	static RenderObject FromJson(const nlohmann::json& data) {
		RenderObject result = RenderObject();
		result.Name = data["name"];
		result.GUID = Guid(data["guid"]);
		result.Mesh = ResourceManager::GetMesh(Guid(data["mesh"]));
		// TODO material is not in resource manager
		//objects[ix]["material"] = obj.Material->GetGUID().str();
		result.Position = ParseJsonVec3(data["position"]);
		result.Rotation = ParseJsonVec3(data["rotation"]);
		result.Scale = ParseJsonVec3(data["scale"]);
		// If we have mesh parameters, we'll use that instead of the existing mesh
		if (data.contains("mesh_params") && data["mesh_params"].is_array()) {
			std::vector<nlohmann::json> meshbuilderParams = data["mesh_params"].get<std::vector<nlohmann::json>>();
			MeshBuilder<VertexPosNormTexCol> mesh;
			for (int ix = 0; ix < meshbuilderParams.size(); ix++) {
				MeshBuilderParam p = MeshBuilderParam::FromJson(meshbuilderParams[ix]);
				result.MeshBuilderParams.push_back(p);
				MeshFactory::AddParameterized(mesh, p);
			}
			result.Mesh = mesh.Bake();
		}
		return result;
	}

	/// <summary>
	/// Converts this object into it's JSON representation for storage
	/// </summary>
	nlohmann::json ToJson() const {
		nlohmann::json result = {
			{ "name", Name },
			{ "guid", GUID.str() },
			{ "mesh", Mesh->GetGUID().str() },
			{ "material", Material->GetGUID().str() },
			{ "position", GlmToJson(Position) },
			{ "rotation", GlmToJson(Rotation) },
			{ "scale", GlmToJson(Scale) },
		};
		if (MeshBuilderParams.size() > 0) {
			std::vector<nlohmann::json> params = std::vector<nlohmann::json>();
			params.resize(MeshBuilderParams.size());
			for (int ix = 0; ix < MeshBuilderParams.size(); ix++) {
				params[ix] = MeshBuilderParams[ix].ToJson();
			}
			result["mesh_params"] = params;
		}
		return result;
	}
};

// Helper structure for our light data
struct Light {
	glm::vec3 Position;
	glm::vec3 Color;
	// Basically inverse of how far our light goes (1/dist, approx)
	float Attenuation = 1.0f / 5.0f;
	// The approximate range of our light
	float Range = 4.0f;

	/// <summary>
	/// Loads a light from a JSON blob
	/// </summary>
	static Light FromJson(const nlohmann::json& data) {
		Light result;
		result.Position = ParseJsonVec3(data["position"]);
		result.Color = ParseJsonVec3(data["color"]);
		result.Range = data["range"].get<float>();
		result.Attenuation = 1.0f / (1.0f + result.Range);
		return result;
	}

	/// <summary>
	/// Converts this object into it's JSON representation for storage
	/// </summary>
	nlohmann::json ToJson() const {
		return {
			{ "position", GlmToJson(Position) },
			{ "color", GlmToJson(Color) },
			{ "range", Range },
		};
	}

};

// Temporary structure for storing all our scene stuffs
struct Scene {
	typedef std::shared_ptr<Scene> Sptr;

	std::unordered_map<Guid, MaterialInfo::Sptr> Materials; // Really should be in resources but meh

	// Stores all the objects in our scene
	std::vector<RenderObject>  Objects;
	// Stores all the lights in our scene
	std::vector<Light>         Lights;
	// The camera for our scene
	Camera::Sptr               Camera;

	Shader::Sptr               BaseShader; // Should think of more elegant ways of handling this

	Scene() :
		Materials(std::unordered_map<Guid, MaterialInfo::Sptr>()),
		Objects(std::vector<RenderObject>()),
		Lights(std::vector<Light>()),
		Camera(nullptr),
		BaseShader(nullptr) {}

	/// <summary>
	/// Searches all render objects in the scene and returns the first
	/// one who's name matches the one given, or nullptr if no object
	/// is found
	/// </summary>
	/// <param name="name">The name of the object to find</param>
	RenderObject* FindObjectByName(const std::string name) {
		auto it = std::find_if(Objects.begin(), Objects.end(), [&](const RenderObject& obj) {
			return obj.Name == name;
			});
		return it == Objects.end() ? nullptr : &(*it);
	}

	/// <summary>
	/// Loads a scene from a JSON blob
	/// </summary>
	static Scene::Sptr FromJson(const nlohmann::json& data) {
		Scene::Sptr result = std::make_shared<Scene>();
		result->BaseShader = ResourceManager::GetShader(Guid(data["default_shader"]));

		LOG_ASSERT(data["materials"].is_array(), "Materials not present in scene!");
		for (auto& material : data["materials"]) {
			MaterialInfo::Sptr mat = MaterialInfo::FromJson(material);
			result->Materials[mat->GetGUID()] = mat;
		}

		LOG_ASSERT(data["objects"].is_array(), "Objects not present in scene!");
		for (auto& object : data["objects"]) {
			RenderObject obj = RenderObject::FromJson(object);
			obj.Material = result->Materials[Guid(object["material"])];
			result->Objects.push_back(obj);
		}

		LOG_ASSERT(data["lights"].is_array(), "Lights not present in scene!");
		for (auto& light : data["lights"]) {
			result->Lights.push_back(Light::FromJson(light));
		}

		// Create and load camera config
		result->Camera = Camera::Create();
		result->Camera->SetPosition(ParseJsonVec3(data["camera"]["position"]));
		result->Camera->SetForward(ParseJsonVec3(data["camera"]["normal"]));

		return result;
	}

	/// <summary>
	/// Converts this object into it's JSON representation for storage
	/// </summary>
	nlohmann::json ToJson() const {
		nlohmann::json blob;
		// Save the default shader (really need a material class)
		blob["default_shader"] = BaseShader->GetGUID().str();

		// Save materials (TODO: this should be managed by the ResourceManager)
		std::vector<nlohmann::json> materials;
		materials.resize(Materials.size());
		int ix = 0;
		for (auto& [key, value] : Materials) {
			materials[ix] = value->ToJson();
			ix++;
		}
		blob["materials"] = materials;

		// Save renderables
		std::vector<nlohmann::json> objects;
		objects.resize(Objects.size());
		for (int ix = 0; ix < Objects.size(); ix++) {
			objects[ix] = Objects[ix].ToJson();
		}
		blob["objects"] = objects;

		// Save lights
		std::vector<nlohmann::json> lights;
		lights.resize(Lights.size());
		for (int ix = 0; ix < Lights.size(); ix++) {
			lights[ix] = Lights[ix].ToJson();
		}
		blob["lights"] = lights;

		// Save camera info
		blob["camera"] = {
			{"position", GlmToJson(Camera->GetPosition()) },
			{"normal",   GlmToJson(Camera->GetForward()) }
		};

		return blob;
	}

	/// <summary>
	/// Saves this scene to an output JSON file
	/// </summary>
	/// <param name="path">The path of the file to write to</param>
	void Save(const std::string& path) {
		// Save data to file
		FileHelpers::WriteContentsToFile(path, ToJson().dump());
		LOG_INFO("Saved scene to \"{}\"", path);
	}

	/// <summary>
	/// Loads a scene from an input JSON file
	/// </summary>
	/// <param name="path">The path of the file to read from</param>
	/// <returns>A new scene loaded from the file</returns>
	static Scene::Sptr Load(const std::string& path) {
		LOG_INFO("Loading scene from \"{}\"", path);
		std::string content = FileHelpers::ReadFile(path);
		nlohmann::json blob = nlohmann::json::parse(content);
		return FromJson(blob);
	}
};

/// <summary>
/// Handles setting the shader uniforms for our light structure in our array of lights
/// </summary>
/// <param name="shader">The pointer to the shader</param>
/// <param name="uniformName">The name of the uniform (ex: u_Lights)</param>
/// <param name="index">The index of the light to set</param>
/// <param name="light">The light data to copy over</param>
void SetShaderLight(const Shader::Sptr& shader, const std::string& uniformName, int index, const Light& light) {
	std::stringstream stream;
	stream << uniformName << "[" << index << "]";
	std::string name = stream.str();

	// Set the shader uniforms for the light
	shader->SetUniform(name + ".Position", light.Position);
	shader->SetUniform(name + ".Color", light.Color);
	shader->SetUniform(name + ".Attenuation", light.Attenuation);
}

/// <summary>
/// Creates the shader and sets up all the lights
/// </summary>
void SetupShaderAndLights(const Shader::Sptr& shader, Light* lights, int numLights) {
	// Global light params
	shader->SetUniform("u_AmbientCol", glm::vec3(0.1f));

	// Send the lights to our shader
	shader->SetUniform("u_NumLights", numLights);
	for (int ix = 0; ix < numLights; ix++) {
		SetShaderLight(shader, "u_Lights", ix, lights[ix]);
	}
}

/// <summary>
/// Draws a widget for saving or loading our scene
/// </summary>
/// <param name="scene">Reference to scene pointer</param>
/// <param name="path">Reference to path string storage</param>
/// <returns>True if a new scene has been loaded</returns>
bool DrawSaveLoadImGui(Scene::Sptr& scene, std::string& path) {
	// Since we can change the internal capacity of an std::string,
	// we can do cool things like this!
	ImGui::InputText("Path", path.data(), path.capacity());

	// Draw a save button, and save when pressed
	if (ImGui::Button("Save")) {
		scene->Save(path);
	}
	ImGui::SameLine();
	// Load scene from file button
	if (ImGui::Button("Load")) {
		// Since it's a reference to a ptr, this will
		// overwrite the existing scene!
		scene = Scene::Load(path);

		return true;
	}
	return false;
}

/// <summary>
/// Draws some ImGui controls for the given light
/// </summary>
/// <param name="title">The title for the light's header</param>
/// <param name="light">The light to modify</param>
/// <returns>True if the parameters have changed, false if otherwise</returns>
bool DrawLightImGui(const char* title, Light& light) {
	bool result = false;
	ImGui::PushID(&light); // We can also use pointers as numbers for unique IDs
	if (ImGui::CollapsingHeader(title)) {
		result |= ImGui::DragFloat3("Pos", &light.Position.x, 0.01f);
		result |= ImGui::ColorEdit3("Col", &light.Color.r);
		result |= ImGui::DragFloat("Range", &light.Range, 0.1f);
	}
	ImGui::PopID();
	if (result) {
		light.Attenuation = 1.0f / (light.Range + 1.0f);
	}
	return result;
}

//////////////////////////////////////////////////////
////////////////// END OF NEW ////////////////////////
//////////////////////////////////////////////////////

int main() {
	Logger::Init(); // We'll borrow the logger from the toolkit, but we need to initialize it

	//Initialize GLFW
	if (!initGLFW())
		return 1;

	//Initialize GLAD
	if (!initGLAD())
		return 1;

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(GlDebugMessage, nullptr);

	// Initialize our ImGui helper
	ImGuiHelper::Init(window);

	// Initialize our resource manager
	ResourceManager::Init();

	// GL states, we'll enable depth testing and backface fulling
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

	// The scene that we will be rendering
	Scene::Sptr scene = nullptr;

	bool loadScene = false;
	// For now we can use a toggle to generate our scene vs load from file
	if (loadScene) {
		ResourceManager::LoadManifest("manifest.json");
		scene = Scene::Load("scene.json");
	}
	else { 
		// Create our OpenGL resources
		Guid defaultShader = ResourceManager::CreateShader({
			{ ShaderPartType::Vertex, "shaders/vertex_shader.glsl" },
			{ ShaderPartType::Fragment, "shaders/frag_blinn_phong_textured.glsl" }
			});

		/*Guid specShader = ResourceManager::CreateShader({
			{ShaderPartType::Vertex, "shader/vertex_shader.glsl"},
			{ShaderPartType::Fragment, "shaders/spec.glsl"}
			}); */

		Guid sphereMesh = ResourceManager::CreateMesh("circle.obj");
		Guid paddleMesh = ResourceManager::CreateMesh("paddle.obj");
		Guid plane = ResourceManager::CreateMesh("background.obj");
		Guid paddleTex = ResourceManager::CreateTexture("textures/paddleTex.jpg");
		Guid ballTex = ResourceManager::CreateTexture("textures/green.jpg");
		Guid brickTex = ResourceManager::CreateTexture("textures/brickTex.jpg");
		Guid backgroundTex = ResourceManager::CreateTexture("textures/background2.png");
		Guid WinTex = ResourceManager::CreateTexture("textures/brickwin.jpeg");
		Guid LossTex = ResourceManager::CreateTexture("textures/brickloss.jpeg");

		// Save the asset manifest for all the resources we just loaded
		ResourceManager::SaveManifest("manifest.json");

		// Create an empty scene
		scene = std::make_shared<Scene>();

		// I hate this
		scene->BaseShader = ResourceManager::GetShader(defaultShader);

		// Create our materials

		MaterialInfo::Sptr ballMaterial = std::make_shared<MaterialInfo>();
		ballMaterial->Shader = scene->BaseShader;
		ballMaterial->Texture = ResourceManager::GetTexture(ballTex);
		ballMaterial->Shininess = 1.0f;
		scene->Materials[ballMaterial->GetGUID()] = ballMaterial;

		MaterialInfo::Sptr paddleMaterial = std::make_shared<MaterialInfo>();
		paddleMaterial->Shader = scene->BaseShader;
		paddleMaterial->Texture = ResourceManager::GetTexture(paddleTex);
		paddleMaterial->Shininess = 1.0f;
		scene->Materials[paddleMaterial->GetGUID()] = paddleMaterial;

		MaterialInfo::Sptr brickMaterial = std::make_shared<MaterialInfo>();
		brickMaterial->Shader = scene->BaseShader;
		brickMaterial->Texture = ResourceManager::GetTexture(brickTex);
		brickMaterial->Shininess = 1.0f;
		scene->Materials[brickMaterial->GetGUID()] = brickMaterial;

		MaterialInfo::Sptr bgMaterial = std::make_shared<MaterialInfo>();
		bgMaterial->Shader = scene->BaseShader;
		bgMaterial->Texture = ResourceManager::GetTexture(backgroundTex);
		bgMaterial->Shininess = 1.0f;
		scene->Materials[bgMaterial->GetGUID()] = bgMaterial;

		MaterialInfo::Sptr winMat = std::make_shared<MaterialInfo>();
		winMat->Shader = scene->BaseShader;
		winMat->Texture = ResourceManager::GetTexture(WinTex);
		winMat->Shininess = 1.0f;
		scene->Materials[winMat->GetGUID()] = winMat;

		MaterialInfo::Sptr lossMat = std::make_shared<MaterialInfo>();
		lossMat->Shader = scene->BaseShader;
		lossMat->Texture = ResourceManager::GetTexture(LossTex);
		lossMat->Shininess = 1.0f;
		scene->Materials[lossMat->GetGUID()] = lossMat;

		// Create some lights for our scene
		scene->Lights.resize(1); 
		scene->Lights[0].Position = glm::vec3(0.0f, 0.0f, -33.0f);
		scene->Lights[0].Color = glm::vec3(20.0f, 20.0f, 20.0f);

		/*scene->Lights[1].Position = glm::vec3(1.0f, 0.0f, -20.0f);
		scene->Lights[1].Color = glm::vec3(0.2f, 0.8f, 0.1f);

		scene->Lights[2].Position = glm::vec3(0.0f, 1.0f, 3.0f);
		scene->Lights[2].Color = glm::vec3(1.0f, 0.2f, 0.1f);

		scene->Lights[3].Position = glm::vec3(0.0f, 0.0f, -15.0f);
		scene->Lights[3].Color = glm::vec3(1.0f, 1.0f, 1.0f);
		scene->Lights[3].Range = 50.0f;
		*/

		// Set up the scene's camera
		scene->Camera = Camera::Create();
		scene->Camera->SetPosition(glm::vec3(0, 0, 9));
		scene->Camera->LookAt(glm::vec3(0.0f));


		// Set up all our sample objects

		RenderObject ball = RenderObject();
		ball.Position = glm::vec3(0.0f, 0.0f, 0.0f);
		ball.Scale = glm::vec3(0.3f, 0.3f, 0.3f);
		ball.Mesh = ResourceManager::GetMesh(sphereMesh);
		ball.Material = ballMaterial;
		ball.Name = "Ball";
		scene->Objects.push_back(ball);

		RenderObject paddle = RenderObject();
		paddle.Position = glm::vec3(0.0f, 5.8f, 0.0f);
		paddle.Rotation = glm::vec3(180.0f, -90.0f, 0.0f);
		paddle.Scale = glm::vec3(1.0f, 0.484f, 0.23f);
		paddle.Mesh = ResourceManager::GetMesh(paddleMesh);
		paddle.Material = paddleMaterial;
		paddle.Name = "Paddle";
		scene->Objects.push_back(paddle);

		RenderObject brick1 = RenderObject();
		brick1.Position = glm::vec3(-4.3f, -4.5f, 0.0f);
		brick1.Scale = glm::vec3(0.7f, 0.7f, 0.7f);
		brick1.Mesh = ResourceManager::GetMesh(sphereMesh);
		brick1.Material = brickMaterial;
		brick1.Name = "Brick 1";
		scene->Objects.push_back(brick1);

		RenderObject brick2 = RenderObject();
		brick2.Position = glm::vec3(-4.3f, -0.52f, 0.0f);
		brick2.Scale = glm::vec3(0.7f, 0.7f, 0.7f);
		brick2.Mesh = ResourceManager::GetMesh(sphereMesh);
		brick2.Material = brickMaterial;
		brick2.Name = "Brick 2";
		scene->Objects.push_back(brick2);

		RenderObject brick3 = RenderObject();
		brick3.Position = glm::vec3(0.0f, -2.5f, 0.0f);
		brick3.Scale = glm::vec3(0.7f, 0.7f, 0.7f);
		brick3.Mesh = ResourceManager::GetMesh(sphereMesh);
		brick3.Material = brickMaterial;
		brick3.Name = "Brick 3";
		scene->Objects.push_back(brick3);

		RenderObject brick4 = RenderObject();
		brick4.Position = glm::vec3(4.3f, -4.5f, 0.0f);
		brick4.Scale = glm::vec3(0.7f, 0.7f, 0.7f);
		brick4.Mesh = ResourceManager::GetMesh(sphereMesh);
		brick4.Material = brickMaterial;
		brick4.Name = "Brick 4";
		scene->Objects.push_back(brick4);

		RenderObject brick5 = RenderObject();
		brick5.Position = glm::vec3(4.3f, -0.52f, 0.0f);
		brick5.Scale = glm::vec3(0.7f, 0.7f, 0.7f);
		brick5.Mesh = ResourceManager::GetMesh(sphereMesh);
		brick5.Material = brickMaterial;
		brick5.Name = "Brick 5";
		scene->Objects.push_back(brick5);

		RenderObject background = RenderObject();
		background.Position = glm::vec3(0.0f, 0.0f, -10.0f);
		background.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
		background.Rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
		background.Mesh = ResourceManager::GetMesh(plane);
		background.Material = bgMaterial;
		background.Name = "back";
		scene->Objects.push_back(background);

		RenderObject Winscreen = RenderObject();
		Winscreen.Position = glm::vec3(0.0f, 0.0f, -50.0f);
		Winscreen.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
		Winscreen.Rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
		Winscreen.Mesh = ResourceManager::GetMesh(plane);
		Winscreen.Material = winMat;
		Winscreen.Name = "winscreen";
		scene->Objects.push_back(Winscreen);

		RenderObject Lossscreen = RenderObject();
		Lossscreen.Position = glm::vec3(0.0f, 0.0f, -50.0f);
		Lossscreen.Scale = glm::vec3(1.0f, 1.0f, 1.0f);
		Lossscreen.Rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
		Lossscreen.Mesh = ResourceManager::GetMesh(plane);
		Lossscreen.Material = lossMat;
		Lossscreen.Name = "lossscreen";
		scene->Objects.push_back(Lossscreen);

		// Save the scene to a JSON file
		scene->Save("scene.json");
	}

	// Post-load setup
	SetupShaderAndLights(scene->BaseShader, scene->Lights.data(), scene->Lights.size());

	RenderObject* ball = scene->FindObjectByName("Ball");
	RenderObject* paddle = scene->FindObjectByName("Paddle");
	RenderObject* brick1 = scene->FindObjectByName("Brick 1");
	RenderObject* brick2 = scene->FindObjectByName("Brick 2");
	RenderObject* brick3 = scene->FindObjectByName("Brick 3");
	RenderObject* brick4 = scene->FindObjectByName("Brick 4");
	RenderObject* brick5 = scene->FindObjectByName("Brick 5");
	RenderObject* backgroundplane = scene->FindObjectByName("back");
	RenderObject* winplane = scene->FindObjectByName("winscreen");
	RenderObject* lossplane = scene->FindObjectByName("lossscreen");

	// We'll use this to allow editing the save/load path
	// via ImGui, note the reserve to allocate extra space
	// for input!
	std::string scenePath = "scene.json";
	scenePath.reserve(256);

	bool isRotating = true;

	// Our high-precision timer
	double lastFrame = glfwGetTime();

	///// Game loop /////
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGuiHelper::StartFrame();

		// Calculate the time since our last frame (dt)
		double thisFrame = glfwGetTime();
		float dt = static_cast<float>(thisFrame - lastFrame);

		// Showcasing how to use the imGui library!
		bool isDebugWindowOpen = ImGui::Begin("Debugging");
		if (isDebugWindowOpen) {
			// Make a checkbox for the monkey rotation
			ImGui::Checkbox("Rotating", &isRotating);

			// Make a new area for the scene saving/loading
			ImGui::Separator();
			if (DrawSaveLoadImGui(scene, scenePath)) {
				// Re-initialize lights, as they may have moved around
				SetupShaderAndLights(scene->BaseShader, scene->Lights.data(), scene->Lights.size());
			}
			ImGui::Separator();
		}



		/////////// UPDATING GAME LOOP /////////////

		//Check for Collisions
		if ((ball->Position.x >= 7.21f) || (ball->Position.x <= -7.21f)) //bounce off left or right wall
			dirX *= -1;
		if (ball->Position.y <= -7.17f) //bounce off top wall
			dirY *= -1;

		//ball hits sphere bricks
		if (calcDist(brick1->Position.x, brick1->Position.y, ball->Position.x, ball->Position.y) <= (brickRadius + radius)) {
			brickCount++;
			speedX = (sqrt(pow((ball->Position.x - brick1->Position.x), 2))) / ((brickRadius + radius) / calcSpeed(speedX, speedY));
			speedY = (sqrt(pow((ball->Position.y - brick1->Position.y), 2))) / ((brickRadius + radius) / calcSpeed(speedX, speedY));

			if (brick1->Position.x > ball->Position.x)
				dirX = -1;
			if (brick1->Position.x < ball->Position.x)
				dirX = 1;
			if (brick1->Position.y > ball->Position.y)
				dirY = -1;
			if (brick1->Position.y < ball->Position.y)
				dirY = 1;

			brick1->Position = glm::vec3(-10.f, 0.0f, 0.0f);
		}

		if (calcDist(brick2->Position.x, brick2->Position.y, ball->Position.x, ball->Position.y) <= (brickRadius + radius)) {
			brickCount++;
			speedX = (sqrt(pow((ball->Position.x - brick2->Position.x), 2))) / ((brickRadius + radius) / calcSpeed(speedX, speedY));
			speedY = (sqrt(pow((ball->Position.y - brick2->Position.y), 2))) / ((brickRadius + radius) / calcSpeed(speedX, speedY));

			if (brick2->Position.x > ball->Position.x)
				dirX = -1;
			if (brick2->Position.x < ball->Position.x)
				dirX = 1;
			if (brick2->Position.y > ball->Position.y)
				dirY = -1;
			if (brick2->Position.y < ball->Position.y)
				dirY = 1;

			brick2->Position = glm::vec3(-10.f, 0.0f, 0.0f);
		}

		if (calcDist(brick3->Position.x, brick3->Position.y, ball->Position.x, ball->Position.y) <= (brickRadius + radius)) {
			brickCount++;
			speedX = (sqrt(pow((ball->Position.x - brick3->Position.x), 2))) / ((brickRadius + radius) / calcSpeed(speedX, speedY));
			speedY = (sqrt(pow((ball->Position.y - brick3->Position.y), 2))) / ((brickRadius + radius) / calcSpeed(speedX, speedY));

			if (brick3->Position.x > ball->Position.x)
				dirX = -1;
			if (brick3->Position.x < ball->Position.x)
				dirX = 1;
			if (brick3->Position.y > ball->Position.y)
				dirY = -1;
			if (brick3->Position.y < ball->Position.y)
				dirY = 1;

			brick3->Position = glm::vec3(-10.f, 0.0f, 0.0f);
		}

		if (calcDist(brick4->Position.x, brick4->Position.y, ball->Position.x, ball->Position.y) <= (brickRadius + radius)) {
			brickCount++;
			speedX = (sqrt(pow((ball->Position.x - brick4->Position.x), 2))) / ((brickRadius + radius) / calcSpeed(speedX, speedY));
			speedY = (sqrt(pow((ball->Position.y - brick4->Position.y), 2))) / ((brickRadius + radius) / calcSpeed(speedX, speedY));

			if (brick4->Position.x > ball->Position.x)
				dirX = -1;
			if (brick4->Position.x < ball->Position.x)
				dirX = 1;
			if (brick4->Position.y > ball->Position.y)
				dirY = -1;
			if (brick4->Position.y < ball->Position.y)
				dirY = 1;

			brick4->Position = glm::vec3(-10.f, 0.0f, 0.0f);
		}

		if (calcDist(brick5->Position.x, brick5->Position.y, ball->Position.x, ball->Position.y) <= (brickRadius + radius)) {
			brickCount++;
			speedX = (sqrt(pow((ball->Position.x - brick5->Position.x), 2))) / ((brickRadius + radius) / calcSpeed(speedX, speedY));
			speedY = (sqrt(pow((ball->Position.y - brick5->Position.y), 2))) / ((brickRadius + radius) / calcSpeed(speedX, speedY));

			if (brick5->Position.x > ball->Position.x)
				dirX = -1;
			if (brick5->Position.x < ball->Position.x)
				dirX = 1;
			if (brick5->Position.y > ball->Position.y)
				dirY = -1;
			if (brick5->Position.y < ball->Position.y)
				dirY = 1;

			brick5->Position = glm::vec3(-10.f, 0.0f, 0.0f);
		}

		//bounce off paddle (changes angle depending on where it hits paddle)
		if (ball->Position.y >= 5.36f
			&& (ball->Position.x >= (paddle->Position.x - 1.44f))
			&& ball->Position.x <= (paddle->Position.x + 1.44f)) {

			dirY = -1;

			if (ball->Position.x < (paddle->Position.x - 0.864f)) { //far left end
				dirX = -1;
				speedX = 0.015f * 2;
				speedY = 0.015f * 2;
			}
			else if (ball->Position.x >= (paddle->Position.x - 0.864f) && ball->Position.x < (paddle->Position.x - 0.288f)) { //left side
				dirX = -1;
				speedX = 0.0061f * 2;
				speedY = 0.0148f * 2;
			}
			else if (ball->Position.x >= (paddle->Position.x - 0.288f) && ball->Position.x <= (paddle->Position.x + 0.288)) { //middle
				dirX = 0;
				speedX = 0.0f;
				speedY = 0.011f * 2;
			}
			else if (ball->Position.x > (paddle->Position.x + 0.288f) && ball->Position.x <= (paddle->Position.x + 0.864f)) { //right side
				dirX = 1;
				speedX = 0.0061f * 2;
				speedY = 0.0148f * 2;
			}
			else if (ball->Position.x > (paddle->Position.x + 0.864f)) { //far right end
				dirX = 1;
				speedX = 0.015f * 2;
				speedY = 0.015f * 2;
			}
		}

		if (ball->Position.y > 6.f) { //player loses (ball goes past paddle)

			speedX = 0.0f;
			speedY = 0.0f;
			lose = true;
		}

		if (brickCount == 5) { //player wins (hits all 5 bricks)
			win = true;
		}

		//Update ball movement
		if (!lose && !win)
			ball->Position += glm::vec3(speedX * dirX /** 2.0f*/, speedY * dirY * 2.0f, 0.f);

		//Update paddle movement
		if (!lose && !win)
			paddle->Position.x = movePaddle(paddle->Position.x);
		 
		//Lose Condition
		if (lose) {
			lossplane->Position = glm::vec3(0.0f, 0.0f, 4.0f);
			winplane->Position = glm::vec3(0.0f, 0.0f, -50.0f);
		}

		//Win Condition
		if (win) {
			winplane->Position = glm::vec3(0.0f, 0.0f, 4.0f);
			lossplane->Position = glm::vec3(0.0f, 0.0f, -50.0f);
		}

		//Swapping Shaders for Lighting Toggles
		/*We made a shader for each of the toggles, but this tutorial framework doesn't support multiple shaders/I couldn't get
		  it to work. To implement this (not saying this would be efficient), I would create materials for each object in the 
		  scene and attach a different frag shader for each type of toggle. 
		  5 types of shaders x 8 objects in game scene = 40 materials.
		  When the player hits a number button, I would swap the materials of each object to showcase a certain shader. 
		  Under each of the toggles, I'll explain what the associated shader does.
		*/
		if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) { //no lighting
			/*"nolight.glsl"
			  the frag_color returns a vec4 with values of 0, so that no colour outputs
			*/
		}

		if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) { //ambient lighting
			/* "diff.glsl"
				made the specular value 0
				we only return the diffuse component (no specular)
			*/
		}

		if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) { //specular lighting
			/* "spec.glsl"
				diffuse value is 0 
				we do not return a texture colour either, so all you see is grayscale shading as highlights
			*/
		}

		if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) { //ambient + specular
			/* "frag_blinn_phong_textured.glsl"
			* default shader by adding specular and diffuse
			*/
		}

		if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) { //turn material off
			/* "notex.glsl"
			*  do no return a texture and apply the lighting from the specular and diffuse
			*/
		}


		// Clear the color and depth buffers
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// Grab shorthands to the camera and shader from the scene
		Shader::Sptr shader = scene->BaseShader;
		Camera::Sptr camera = scene->Camera;

		camera->SetOrthoVerticalScale(15.0f);
		camera->SetOrthoEnabled(true);

		// Bind our shader for use
		shader->Bind();

		// Update our application level uniforms every frame
		shader->SetUniform("u_CamPos", scene->Camera->GetPosition());

		// Draw some ImGui stuff for the lights
		if (isDebugWindowOpen) {
			for (int ix = 0; ix < scene->Lights.size(); ix++) {
				char buff[256];
				sprintf_s(buff, "Light %d##%d", ix, ix);
				if (DrawLightImGui(buff, scene->Lights[ix])) {
					SetShaderLight(shader, "u_Lights", ix, scene->Lights[ix]);
				}
			}
			// Split lights from the objects in ImGui
			ImGui::Separator();
		}


		// Render all our objects
		for (int ix = 0; ix < scene->Objects.size(); ix++) {
			RenderObject* object = &scene->Objects[ix];

			// Update the object's transform for rendering
			object->RecalcTransform();

			// Set vertex shader parameters
			shader->SetUniformMatrix("u_ModelViewProjection", camera->GetViewProjection() * object->Transform);
			shader->SetUniformMatrix("u_Model", object->Transform);
			shader->SetUniformMatrix("u_NormalMatrix", glm::mat3(glm::transpose(glm::inverse(object->Transform))));

			// Apply this object's material
			object->Material->Apply();

			// Draw the object
			object->Mesh->Draw();

			// If our debug window is open, then let's draw some info for our objects!
			if (isDebugWindowOpen) {
				// All these elements will go into the last opened window
				if (ImGui::CollapsingHeader(object->Name.c_str())) {
					ImGui::PushID(ix); // Push a new ImGui ID scope for this object
					ImGui::DragFloat3("Position", &object->Position.x, 0.01f);
					ImGui::DragFloat3("Rotation", &object->Rotation.x, 1.0f);
					ImGui::DragFloat3("Scale", &object->Scale.x, 0.01f, 0.0f);
					ImGui::PopID(); // Pop the ImGui ID scope for the object
				}
			}
		}

		// If our debug window is open, notify that we no longer will render new
		// elements to it
		if (isDebugWindowOpen) {
			ImGui::End();
		}

		VertexArrayObject::Unbind();

		lastFrame = thisFrame;
		ImGuiHelper::EndFrame();
		glfwSwapBuffers(window);
	}

	// Clean up the ImGui library
	ImGuiHelper::Cleanup();  

	// Clean up the resource manager
	ResourceManager::Cleanup();

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}