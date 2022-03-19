#include "DefaultSceneLayer.h"

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)

#include <filesystem>

// Graphics
#include "Graphics/Buffers/IndexBuffer.h"
#include "Graphics/Buffers/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture2D.h"
#include "Graphics/Textures/TextureCube.h"
#include "Graphics/VertexTypes.h"
#include "Graphics/Font.h"
#include "Graphics/GuiBatcher.h"
#include "Graphics/Framebuffer.h"

// Utilities
#include "Utils/MeshBuilder.h"
#include "Utils/MeshFactory.h"
#include "Utils/ObjLoader.h"
#include "Utils/ImGuiHelper.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/FileHelpers.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/StringUtils.h"
#include "Utils/GlmDefines.h"

// Gameplay
#include "Gameplay/Material.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"

// Components
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Components/Camera.h"
#include "Gameplay/Components/RotatingBehaviour.h"
#include "Gameplay/Components/JumpBehaviour.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Components/MaterialSwapBehaviour.h"
#include "Gameplay/Components/TriggerVolumeEnterBehaviour.h"
#include "Gameplay/Components/SimpleCameraControl.h"

// Physics
#include "Gameplay/Physics/RigidBody.h"
#include "Gameplay/Physics/Colliders/BoxCollider.h"
#include "Gameplay/Physics/Colliders/PlaneCollider.h"
#include "Gameplay/Physics/Colliders/SphereCollider.h"
#include "Gameplay/Physics/Colliders/ConvexMeshCollider.h"
#include "Gameplay/Physics/Colliders/CylinderCollider.h"
#include "Gameplay/Physics/TriggerVolume.h"
#include "Graphics/DebugDraw.h"

// GUI
#include "Gameplay/Components/GUI/RectTransform.h"
#include "Gameplay/Components/GUI/GuiPanel.h"
#include "Gameplay/Components/GUI/GuiText.h"
#include "Gameplay/InputEngine.h"

#include "Application/Application.h"
#include "Gameplay/Components/ParticleSystem.h"
#include "Graphics/Textures/Texture3D.h"
#include "Graphics/Textures/Texture1D.h"

DefaultSceneLayer::DefaultSceneLayer() :
	ApplicationLayer()
{
	Name = "Default Scene";
	Overrides = AppLayerFunctions::OnAppLoad;
}

DefaultSceneLayer::~DefaultSceneLayer() = default;

void DefaultSceneLayer::OnAppLoad(const nlohmann::json& config) {
	_CreateScene();
}

void DefaultSceneLayer::_CreateScene()
{
	using namespace Gameplay;
	using namespace Gameplay::Physics;

	Application& app = Application::Get();

	bool loadScene = false;
	// For now we can use a toggle to generate our scene vs load from file
	if (loadScene && std::filesystem::exists("scene.json")) {
		app.LoadScene("scene.json");
	} else {
		// This time we'll have 2 different shaders, and share data between both of them using the UBO
		// This shader will handle reflective materials 
		ShaderProgram::Sptr reflectiveShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_environment_reflective.glsl" }
		});
		reflectiveShader->SetDebugName("Reflective");

		// This shader handles our basic materials without reflections (cause they expensive)
		ShaderProgram::Sptr basicShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_blinn_phong_textured.glsl" }
		});
		basicShader->SetDebugName("Blinn-phong");

		// This shader handles our cel shading example
		ShaderProgram::Sptr toonShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/toon_shading.glsl" }
		});
		toonShader->SetDebugName("Toon Shader");

		// Load in the meshes
		MeshResource::Sptr GemMesh = ResourceManager::CreateAsset<MeshResource>("Gem.obj");

		// Load in some textures
		Texture2D::Sptr    boxTexture   = ResourceManager::CreateAsset<Texture2D>("textures/box-diffuse.png");
		Texture2D::Sptr    boxSpec      = ResourceManager::CreateAsset<Texture2D>("textures/box-specular.png");
		Texture2D::Sptr    monkeyTex    = ResourceManager::CreateAsset<Texture2D>("textures/monkey-uvMap.png");
		Texture2D::Sptr    GemTex		= ResourceManager::CreateAsset<Texture2D>("textures/Gem.png");
		Texture2D::Sptr    BlueTex = ResourceManager::CreateAsset<Texture2D>("textures/Blue.jpg");
		Texture2D::Sptr    CyanTex = ResourceManager::CreateAsset<Texture2D>("textures/Cyan.png");
		Texture2D::Sptr    MagentaTex = ResourceManager::CreateAsset<Texture2D>("textures/Magenta.jpg");
		Texture2D::Sptr    RedTex = ResourceManager::CreateAsset<Texture2D>("textures/Red.png");
		Texture2D::Sptr    YellowTex = ResourceManager::CreateAsset<Texture2D>("textures/Yellow.png");
		Texture2D::Sptr    WhiteTex = ResourceManager::CreateAsset<Texture2D>("textures/White.png");
		Texture2D::Sptr    GrassTex      = ResourceManager::CreateAsset<Texture2D>("textures/GrassTex.jpg");
		GrassTex->SetMinFilter(MinFilter::NearestMipLinear);
		GrassTex->SetMagFilter(MagFilter::Linear);


		// Loading in a 1D LUT
		Texture1D::Sptr toonLut = ResourceManager::CreateAsset<Texture1D>("luts/toon-1D.png"); 
		toonLut->SetWrap(WrapMode::ClampToEdge);

		// Here we'll load in the cubemap, as well as a special shader to handle drawing the skybox
		TextureCube::Sptr testCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/ocean/ocean.jpg");
		ShaderProgram::Sptr      skyboxShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }
		});

		// Create an empty scene
		Scene::Sptr scene = std::make_shared<Scene>(); 

		// Setting up our enviroment map
		scene->SetSkyboxTexture(testCubemap); 
		scene->SetSkyboxShader(skyboxShader);
		// Since the skybox I used was for Y-up, we need to rotate it 90 deg around the X-axis to convert it to z-up 
		scene->SetSkyboxRotation(glm::rotate(MAT4_IDENTITY, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));

		// Loading in a color lookup table
		Texture3D::Sptr lut = ResourceManager::CreateAsset<Texture3D>("luts/warm.CUBE");  
		// Configure the color correction LUT 1
		scene->SetColorLUT(lut, 0);

		lut = ResourceManager::CreateAsset<Texture3D>("luts/cool.CUBE");
		// Configure the color correction LUT 2
		scene->SetColorLUT(lut, 1);

		lut = ResourceManager::CreateAsset<Texture3D>("luts/Horror.CUBE");
		// Configure the color correction LUT 3
		scene->SetColorLUT(lut, 2);

		// Create our materials
		// This will be our box material, with no environment reflections
		Material::Sptr boxMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			boxMaterial->Name = "Box";
			boxMaterial->Set("u_Material.Diffuse", GrassTex);
			boxMaterial->Set("u_Material.Shininess", 0.1f);
		}


		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr monkeyMaterial = ResourceManager::CreateAsset<Material>(reflectiveShader);
		{
			monkeyMaterial->Name = "Monkey";
			monkeyMaterial->Set("u_Material.Diffuse", monkeyTex);
			monkeyMaterial->Set("u_Material.Shininess", 0.5f);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr GemMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			GemMaterial->Name = "Gem";
			GemMaterial->Set("u_Material.Diffuse", GemTex);
			GemMaterial->Set("u_Material.Specular", boxSpec);
		}
		Material::Sptr RedMaterial = GemMaterial->Clone();
		{
			RedMaterial->Set("u_Material.Diffuse", RedTex);
		}
		Material::Sptr YellowMaterial = GemMaterial->Clone();
		{
			YellowMaterial->Set("u_Material.Diffuse", YellowTex);
		}
		Material::Sptr BlueMaterial = GemMaterial->Clone();
		{
			BlueMaterial->Set("u_Material.Diffuse", BlueTex);
		}
		Material::Sptr CyanMaterial = GemMaterial->Clone();
		{
			CyanMaterial->Set("u_Material.Diffuse", CyanTex);
		}
		Material::Sptr MagentaMaterial = GemMaterial->Clone();
		{
			MagentaMaterial->Set("u_Material.Diffuse", MagentaTex);
		}
		Material::Sptr WhiteMaterial = GemMaterial->Clone();
		{
			WhiteMaterial->Set("u_Material.Diffuse", WhiteTex);
		}


		// Create some lights for our scene
		scene->Lights.resize(3);
		scene->Lights[0].Position = glm::vec3(0.0f, 1.0f, 3.0f);
		scene->Lights[0].Color = glm::vec3(1.0f, 1.0f, 1.0f);
		scene->Lights[0].Range = 100.0f;

		scene->Lights[1].Position = glm::vec3(1.0f, 0.0f, 3.0f);
		scene->Lights[1].Color = glm::vec3(0.2f, 0.8f, 0.1f);

		scene->Lights[2].Position = glm::vec3(0.0f, 1.0f, 3.0f);
		scene->Lights[2].Color = glm::vec3(1.0f, 0.2f, 0.1f);

		// We'll create a mesh that is a simple plane that we can resize later
		MeshResource::Sptr planeMesh = ResourceManager::CreateAsset<MeshResource>();
		planeMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(1.0f)));
		planeMesh->GenerateMesh();

		// Set up the scene's camera
		GameObject::Sptr camera = scene->MainCamera->GetGameObject()->SelfRef();
		{
			camera->SetPostion({ -4.5, 5.5, 4 });
			camera->SetRotation({80,0,-90});

			camera->Add<SimpleCameraControl>();
		}

		// Set up all our sample objects
		GameObject::Sptr plane = scene->CreateGameObject("Plane");
		{
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane->Add<RenderComponent>();
			renderer->SetMesh(tiledMesh);
			renderer->SetMaterial(boxMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}

		GameObject::Sptr demoBase = scene->CreateGameObject("Chaos Emeralds");

		GameObject::Sptr Gem1 = scene->CreateGameObject("Red Gem");
		{
			// Set position in the scene
			Gem1->SetPostion(glm::vec3(8.0f, 0.0f, 1.0f));
			Gem1->SetRotation(glm::vec3(90, 0, 0));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = Gem1->Add<RenderComponent>();
			renderer->SetMesh(GemMesh);
			renderer->SetMaterial(RedMaterial);

			RotatingBehaviour::Sptr movement = Gem1->Add<RotatingBehaviour>(0.5f);

			demoBase->AddChild(Gem1);
		}
		GameObject::Sptr Gem2 = scene->CreateGameObject("Yellow Gem");
		{
			// Set position in the scene
			Gem2->SetPostion(glm::vec3(6.0f, 2.0f, 1.0f));
			Gem2->SetRotation(glm::vec3(90, 0, 0));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = Gem2->Add<RenderComponent>();
			renderer->SetMesh(GemMesh);
			renderer->SetMaterial(YellowMaterial);

			RotatingBehaviour::Sptr movement = Gem2->Add<RotatingBehaviour>(1.0f);

			demoBase->AddChild(Gem2);
		}
		GameObject::Sptr Gem3 = scene->CreateGameObject("Green Gem");
		{
			// Set position in the scene
			Gem3->SetPostion(glm::vec3(4.0f, 4.0f, 1.0f));
			Gem3->SetRotation(glm::vec3(90, 0, 0));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = Gem3->Add<RenderComponent>();
			renderer->SetMesh(GemMesh);
			renderer->SetMaterial(GemMaterial);

			RotatingBehaviour::Sptr movement = Gem3->Add<RotatingBehaviour>(1.5f);

			demoBase->AddChild(Gem3);

		}
		GameObject::Sptr Gem4 = scene->CreateGameObject("Cyan Gem");
		{
			// Set position in the scene
			Gem4->SetPostion(glm::vec3(2.0f, 6.0f, 1.0f));
			Gem4->SetRotation(glm::vec3(90, 0, 0));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = Gem4->Add<RenderComponent>();
			renderer->SetMesh(GemMesh);
			renderer->SetMaterial(CyanMaterial);

			RotatingBehaviour::Sptr movement = Gem4->Add<RotatingBehaviour>(2.0f);

			demoBase->AddChild(Gem4);
		}
		GameObject::Sptr Gem5 = scene->CreateGameObject("Blue Gem");
		{
			// Set position in the scene
			Gem5->SetPostion(glm::vec3(4.0f, 8.0f, 1.0f));
			Gem5->SetRotation(glm::vec3(90, 0, 0));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = Gem5->Add<RenderComponent>();
			renderer->SetMesh(GemMesh);
			renderer->SetMaterial(BlueMaterial);

			RotatingBehaviour::Sptr movement = Gem5->Add<RotatingBehaviour>(2.5f);

			demoBase->AddChild(Gem5);

		}
		GameObject::Sptr Gem6 = scene->CreateGameObject("Purple Gem");
		{
			// Set position in the scene
			Gem6->SetPostion(glm::vec3(6.0f, 10.0f, 1.0f));
			Gem6->SetRotation(glm::vec3(90, 0, 0));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = Gem6->Add<RenderComponent>();
			renderer->SetMesh(GemMesh);
			renderer->SetMaterial(MagentaMaterial);

			RotatingBehaviour::Sptr movement = Gem6->Add<RotatingBehaviour>(3.0f);

			demoBase->AddChild(Gem6);

		}

		GameObject::Sptr Gem7 = scene->CreateGameObject("White Gem");
		{
			// Set position in the scene
			Gem7->SetPostion(glm::vec3(8.0f, 12.0f, 1.0f));
			Gem7->SetRotation(glm::vec3(90, 0, 0));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = Gem7->Add<RenderComponent>();
			renderer->SetMesh(GemMesh);
			renderer->SetMaterial(WhiteMaterial);

			RotatingBehaviour::Sptr movement = Gem7->Add<RotatingBehaviour>(3.5f);

			demoBase->AddChild(Gem7);

		}

		/////////////////////////// UI //////////////////////////////
		/*
		GameObject::Sptr canvas = scene->CreateGameObject("UI Canvas");
		{
			RectTransform::Sptr transform = canvas->Add<RectTransform>();
			transform->SetMin({ 16, 16 });
			transform->SetMax({ 256, 256 });

			GuiPanel::Sptr canPanel = canvas->Add<GuiPanel>();


			GameObject::Sptr subPanel = scene->CreateGameObject("Sub Item");
			{
				RectTransform::Sptr transform = subPanel->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 128, 128 });

				GuiPanel::Sptr panel = subPanel->Add<GuiPanel>();
				panel->SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

				panel->SetTexture(ResourceManager::CreateAsset<Texture2D>("textures/upArrow.png"));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 16.0f);
				font->Bake();

				GuiText::Sptr text = subPanel->Add<GuiText>();
				text->SetText("Hello world!");
				text->SetFont(font);

				monkey1->Get<JumpBehaviour>()->Panel = text;
			}

			canvas->AddChild(subPanel);
		}
		*/

		GuiBatcher::SetDefaultTexture(ResourceManager::CreateAsset<Texture2D>("textures/ui-sprite.png"));
		GuiBatcher::SetDefaultBorderRadius(8);

		// Save the asset manifest for all the resources we just loaded
		ResourceManager::SaveManifest("scene-manifest.json");
		// Save the scene to a JSON file
		scene->Save("scene.json");

		// Send the scene to the application
		app.LoadScene(scene);
	}
}
