#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include<glm/glm.hpp>
#include<gl/glew.h>
#include <iostream>
#include <sstream>
#include <SDL2/SDL_image.h>

#include <glm/ext.hpp>
#include <vector>
#include<fstream>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags
#define FBDEFINITION 2000
#define WIDTH 400
#define HEIGHT 400
/*
void PrintError() 
{
	std::cout << glewGetErrorString(glGetError())<<"\n";
}
*/
struct InitInfo 
{
	SDL_Window* window;
	SDL_GLContext context;
	InitInfo() = default;
	InitInfo(SDL_Window* _window, SDL_GLContext _context) : window(_window), context(_context) {}
};

struct Vertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uvCoords;
	Vertex() = default;
	Vertex(glm::vec3 pos, glm::vec3 norm, glm::vec2 uv) :position(pos),normal(norm), uvCoords(uv) {}
};

//loads sdl, creates window and gl context, and  loads glew functions
InitInfo InitializeLibraries() 
{
	if(!SDL_Init(SDL_INIT_EVERYTHING))
	{
	//success
		std::cout << "SDL loaded successfully!\n";
	}
	else 
	{
	//error
		std::cout << "Error loading SDL!\n\nExiting...";
		exit(-1);
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4); 
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2); 


	//TODO: Change title and size
	SDL_Window* window = SDL_CreateWindow("My Window",SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,WIDTH,HEIGHT,SDL_WINDOW_RESIZABLE|SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(window);
	SDL_GL_MakeCurrent(window,context);

	//now that window and context is created, gl functions can be loaded with glew
	if (glewInit() == GLEW_OK) 
	{
	//success
		std::cout << "GLEW loaded successfully!\n";
	}
	else 
	{
		//error
		std::cout << "Error loading GLEW.\n\nExiting...";
		exit(-2);
	}
	return InitInfo(window, context);
}

void ActivateSDLImage() 
{
	if(!IMG_Init(IMG_INIT_PNG| IMG_INIT_JPG) )
	{
		std::cout << "Error loading SDL_Image.\n\nExiting...";
		exit(-5);
	}
	else 
	{
		std::cout << "SDL_Image loaded sucessfully.\n";
	}

}

struct TargetCamera
{
	float fov;
	float width, height;
	glm::vec3 position;
	glm::vec3 target;
	glm::vec3 up;
	TargetCamera(float _fov = 0.4, float w = 100.0f, float h = 100.0f, glm::vec3 pos = { 0,0,-5 }, glm::vec3 _target = { 0,0,0 }, glm::vec3 _up = { 0,1,0 }) :fov(_fov), width(w), height(h), position(pos), target(_target), up(_up)
	{}
};

typedef GLuint Shader;
typedef GLuint Texture;
typedef GLuint VAO;
typedef GLuint VBO;
typedef GLuint FBO;


Texture CreateTexture(unsigned int w,unsigned int h,GLenum internalFormat=GL_DEPTH_COMPONENT, GLenum format = GL_DEPTH_COMPONENT,GLenum type= GL_FLOAT)
{
	Texture texture;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexImage2D(GL_TEXTURE_2D,0,internalFormat,w,h,0,format,type,NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return texture;

}

typedef GLuint RBO;

struct SingleTextureFramebuffer
{
	Texture texture;
	FBO fbo;
	RBO rbo;
SingleTextureFramebuffer(Texture tex,FBO _fbo,RBO _rbo):texture(tex),fbo(_fbo),rbo(_rbo) {}
};

SingleTextureFramebuffer CreateFramebuffer(unsigned int w,unsigned int h,GLenum attachment,GLenum internalFormat = GL_DEPTH_COMPONENT, GLenum format = GL_DEPTH_COMPONENT,GLenum type=GL_FLOAT)
{
	FBO fbo;
	glGenFramebuffers(1, &fbo);
	Texture tex = CreateTexture(w,h,internalFormat,format,type);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	glFramebufferTexture2D(GL_FRAMEBUFFER,attachment,GL_TEXTURE_2D,tex, 0);
	if (attachment == GL_DEPTH_ATTACHMENT) {
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}
	glBindTexture(GL_TEXTURE_2D,0);
	
	RBO rbo=0;
	if (attachment != GL_DEPTH_ATTACHMENT) {

		glGenRenderbuffers(1, &rbo);
		glBindRenderbuffer(GL_RENDERBUFFER, rbo);
		glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT,w,h);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,rbo);
	
	}
	
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	

	return{ tex,fbo,rbo };

}



struct Light 
{
	TargetCamera cam;
	SingleTextureFramebuffer fbo;
	Light(TargetCamera camera = { 0.6,FBDEFINITION/100,FBDEFINITION/100,{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f},{0.0f,1.0f,0.0f}}) :cam(camera), fbo(CreateFramebuffer(FBDEFINITION, FBDEFINITION, GL_COLOR_ATTACHMENT0,GL_R16F,GL_RGB)) {}
};



struct Mesh { unsigned int offsetInIndexArray,numberOfIndices;
Mesh() = default;
Mesh(unsigned int offset, unsigned int numIndices):offsetInIndexArray(offset),numberOfIndices(numIndices) {}
};


struct GraphicsInfo 
{
	VAO vao;
	VBO vbo,ibo;
	Shader meshShader, lightShader;
	Texture paletteTexture;

	std::vector<Mesh> meshes;
	Shader fullScreenQuadShader;

	GraphicsInfo() = default;
	GraphicsInfo(GLuint _vao, GLuint _vbo, GLuint _ibo, GLuint _meshShader, GLuint _lightShader, GLuint _paletteTexture,std::vector<Mesh> _meshes,Shader screenQuad) :vao(_vao),vbo(_vbo),ibo(_ibo),lightShader(_lightShader),meshShader(_meshShader),paletteTexture(_paletteTexture),meshes(_meshes),fullScreenQuadShader(screenQuad)
	{
	
	}
};




struct MeshData 
{
	std::vector<Mesh>meshes;
	std::vector<unsigned int> indices;
	std::vector<Vertex> vertices;
	MeshData()=default;
};
//returns mesh data using assimp on a list of files
MeshData GetAllMeshData()
{
	const std::string files [] = {"Housies.dae"};
	// Create an instance of the Importer class
	Assimp::Importer importer;
	unsigned int lastIndex=0;
	MeshData data;
	for (auto& pFile : files) {
	
		const aiScene* scene = importer.ReadFile(pFile,aiProcess_JoinIdenticalVertices|aiProcess_FlipUVs|aiProcess_Triangulate);
		

		if ( scene==NULL) {
			std::cout << "Could not load mesh: " + pFile << "\n\nExiting...";
			exit(-3);
		}
		else 
		{
			std::cout << "Mesh " + pFile << " loaded.\n";
		}
		// Now we can access the file's contents.
		
		
		for (int m = 0;m<scene->mNumMeshes; m++) 
		{
			auto currentIndex=lastIndex;
			auto& mesh = *scene->mMeshes[m];
			for (int v = 0; v < mesh.mNumVertices; v++)
			{
				auto vert = mesh.mVertices[v];
				auto uv = mesh.mTextureCoords[0][v];
				auto normal = mesh.mNormals[v];
				data.vertices.push_back(Vertex({ vert.x,vert.y,vert.z },
					{normal.x,normal.y,normal.z},
					{uv.x, uv.y}));
			}
			for (unsigned int f = 0; f < mesh.mNumFaces; f++) 
			{
				auto face = mesh.mFaces[f];
			
				for (unsigned int i = 0; i < face.mNumIndices; i++) 
				{
					data.indices.push_back(currentIndex+face.mIndices[i]);
					lastIndex++;
				}
			}
			data.meshes.push_back(Mesh(currentIndex,lastIndex-currentIndex));
			
		}
	}

	return data;
}

//creates shader program
Shader CreateShader(std::string name)
{
	auto vertexPath = name + ".vert";
	auto fragmentPath = name + ".frag";
	//need to create the individual vertex and fragment shaders
	Shader shader;

	std::string vertexCode;
	std::string fragmentCode;
	std::ifstream vShaderFile;
	std::ifstream fShaderFile;
	// ensure ifstream objects can throw exceptions:
	vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	try
	{
		// open files
		vShaderFile.open(vertexPath);
		fShaderFile.open(fragmentPath);
		std::stringstream vShaderStream, fShaderStream;
		// read file's buffer contents into streams
		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();
		// close file handlers
		vShaderFile.close();
		fShaderFile.close();
		// convert stream into string
		vertexCode = vShaderStream.str();
		fragmentCode = fShaderStream.str();
	}
	catch (std::ifstream::failure e)
	{
		std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
	}
	const char* vShaderCode = vertexCode.c_str();
	const char* fShaderCode = fragmentCode.c_str();
	

		// 2. compile shaders
		unsigned int vertex, fragment;
	int success;
	char infoLog[512];

	// vertex Shader
	vertex = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex, 1, &vShaderCode, NULL);
	glCompileShader(vertex);
	// print compile errors if any
	glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertex, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	// similiar for Fragment Shader
	
	// fragment Shader
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fShaderCode, NULL);
	glCompileShader(fragment);
	// print compile errors if any
	glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragment, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
		exit(-4);
	};


	// shader Program
	shader = glCreateProgram();
	glAttachShader(shader, vertex);
	glAttachShader(shader, fragment);
	glLinkProgram(shader);
	// print linking errors if any
	glGetProgramiv(shader, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(shader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
		exit(-4);
	}

	// delete the shaders as they're linked into our program now and no longer necessary
	glDeleteShader(vertex);
	glDeleteShader(fragment);

	std::cout << "Created shader \"" + name + "\".\n";

	return shader;
}

//creates 2d texture
Texture CreateTexture(std::string name) 
{
	
	auto img = IMG_Load((name + ".png").c_str());
	
	if (img==nullptr)
	{
		std::cout << "Couldn't load image " + name + ".\n\nExiting...";
		exit(-6);
	}
	else { std::cout << "Image " + name + " loaded successfully!\n"; }
	auto data = (unsigned char*)img->pixels;
	std::vector<unsigned char> finalData;
	

	auto w = img->w, h = img->h;
	
	Texture texture;
	glCreateTextures(GL_TEXTURE_2D,1,&texture);
	glBindTexture(GL_TEXTURE_2D,texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	SDL_FreeSurface(img);
	return texture;
}
GraphicsInfo SetUpGraphics() 
{
	//enable depth
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(true);


	//create vao handle
	VAO vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	//create data to upload to vbo
	auto vertexData = GetAllMeshData();
	
	//create full screen quad ->not working
	auto originalSize = vertexData.vertices.size();
	vertexData.vertices.push_back(Vertex({ -1,-1,0.5f }, { 0,0,0 }, {0,0}));
	vertexData.vertices.push_back(Vertex({ -1,1,0.5f }, { 0,0,0 }, { 0,1.0f }));
	vertexData.vertices.push_back(Vertex({ 1,1,0.5f }, { 0,0,0 }, { 1.0f,1.0f }));
	vertexData.vertices.push_back(Vertex({ 1,-1,0.5f }, { 0,0,0 }, { 1.0f,0 }));
	
	vertexData.indices.push_back(originalSize+0);
	vertexData.indices.push_back(originalSize + 1);
	vertexData.indices.push_back(originalSize + 2);
	vertexData.indices.push_back(originalSize + 2);
	vertexData.indices.push_back(originalSize + 0);
	vertexData.indices.push_back(originalSize + 3);
	
	vertexData.meshes.push_back(Mesh(vertexData.meshes[vertexData.meshes.size() - 1].offsetInIndexArray + vertexData.meshes[vertexData.meshes.size() - 1].numberOfIndices,6));
	//

	//create the vbo and ibo
	VBO vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER,vbo);

	glBufferData(GL_ARRAY_BUFFER,sizeof(Vertex)*vertexData.vertices.size(),(void*) & (vertexData.vertices[0]),GL_STATIC_DRAW);

	VBO ibo;
	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

	glBufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(unsigned int)*vertexData.indices.size(),(void*)& vertexData.indices[0],GL_STATIC_DRAW);

	//setting vao attrib pointers
	glVertexAttribPointer(0,3,GL_FLOAT,false,sizeof(Vertex),0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vertex), (void*)sizeof(glm::vec3));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(Vertex), (void*)(sizeof(glm::vec3)*2));
	glEnableVertexAttribArray(2);

	const auto lightShader =CreateShader("Light");
	const auto screenQuadShader = CreateShader("FullScreenQuadShader");
	const auto meshShader=CreateShader("Mesh");


	ActivateSDLImage();
	const auto texture = CreateTexture("Palette1");

	return GraphicsInfo(vao,vbo,ibo,meshShader,lightShader,texture,vertexData.meshes,screenQuadShader);
}

//assumes shaders and vao/vbo are set
void DrawMesh(Mesh const& mesh) 
{
	glDrawElements(GL_TRIANGLES,mesh.numberOfIndices,GL_UNSIGNED_INT,(void*)(mesh.offsetInIndexArray*sizeof(unsigned int)));
}

//not working
void UseCamera(std::string shaderUniformName,Shader program ,TargetCamera const& cam) 
{
	//may need to switch around

	auto matrix = glm::perspectiveFov(cam.fov,cam.width,cam.height,0.001f,1000.0f)* glm::lookAt(cam.position, cam.target, cam.up);
	auto name = shaderUniformName.c_str();

	auto location = glGetUniformLocation(program,name);
	
	glProgramUniformMatrix4fv(program,location,1,false,(float*) &matrix[0]);
}


struct InputTracker 
{
	float horizontalAxis=0.0f;
	bool rightPressed=false;
	bool leftPressed=false;


	float horizontalAxis2 = 0.0f;
	bool dPressed = false;
	bool aPressed = false;

	bool spacePressed = false;
};

class Clock 
{
	double lastSavedTime;
public: 
	float Restart() 
	{
		auto time = SDL_GetTicks64();
			auto delta = time - lastSavedTime;
		lastSavedTime = time;
		return delta;
	}
	Clock() { lastSavedTime = SDL_GetTicks64(); }
};

void ProcessEvents(SDL_Window* window, bool& running,InputTracker& input,float delta)
{
	SDL_Event event;
	while(SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_QUIT:
			running = false;
			break;
		case SDL_KEYDOWN:
		{auto key = event.key;
		if (key.keysym.sym==SDLK_RIGHT)
		{
			input.rightPressed = true;

		}if (key.keysym.sym == SDLK_LEFT)
		{
			input.leftPressed = true;
		}
		if (key.keysym.sym == SDLK_d)
		{
			input.dPressed = true;
		}if (key.keysym.sym == SDLK_a)
		{
			input.aPressed = true;
		}if (key.keysym.sym == SDLK_SPACE)
		{
			input.spacePressed = true;
		}
		}
		break;
		case SDL_KEYUP:
		{auto key = event.key;
		if (key.keysym.sym == SDLK_RIGHT)
		{
			input.rightPressed = false;
		}if (key.keysym.sym == SDLK_LEFT)
		{
			input.leftPressed = false;
		}
		if (key.keysym.sym == SDLK_d)
		{
			input.dPressed = false;
		}
		if (key.keysym.sym == SDLK_a)
		{
			input.aPressed = false;
		}if (key.keysym.sym == SDLK_SPACE)
		{
			input.spacePressed = false;
		}
		}
		break;
		default:
			break;
		
		}
	}

	if (input.leftPressed||input.rightPressed)
	{
		input.horizontalAxis = std::clamp(input.horizontalAxis-(input.rightPressed - input.leftPressed) * delta,-1.0f,1.0f);
	
	}
	else
	{
		if (input.horizontalAxis>0)
		{
			input.horizontalAxis = std::clamp(input.horizontalAxis - delta, 0.0f, 1.0f);
		}
		if (input.horizontalAxis < 0)
		{
			input.horizontalAxis = std::clamp(input.horizontalAxis+delta, -1.0f, 0.0f);
		}
	}

	
	if (input.aPressed || input.dPressed)
	{
		input.horizontalAxis2 = std::clamp(input.horizontalAxis2 - (input.dPressed - input.aPressed) * delta, -1.0f, 1.0f);

	}
	else
	{
		if (input.horizontalAxis2 > 0)
		{
			input.horizontalAxis2 = std::clamp(input.horizontalAxis2 - delta, 0.0f, 1.0f);
		}
		if (input.horizontalAxis2 < 0)
		{
			input.horizontalAxis2 = std::clamp(input.horizontalAxis2 + delta, -1.0f, 0.0f);
		}
	}



}

void SetUniform(Shader shader,std::string name, int value) 
{

	
	auto location = glGetUniformLocation(shader, name.c_str());


	glProgramUniform1i(shader, location,  value);
}



void main()
{
	const auto initInfo = InitializeLibraries();

	const auto graphicsInfo = SetUpGraphics();

	TargetCamera cam;
	Light light;
	light.cam.position = { 1,0.5f,1 };
	light.cam.target = { 0,0,0 };
	
	float counter = 0;
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	cam.position = { 10,1,-10 };
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	bool running = true;
	InputTracker inputs;
	Clock clock;
	float lCounter = 0.0f;
	float rotateSpeed = 0.01f;
	SingleTextureFramebuffer fb = CreateFramebuffer(WIDTH,HEIGHT,GL_COLOR_ATTACHMENT0,GL_RGBA,GL_RGBA,GL_UNSIGNED_BYTE);
	while (running)
	{
		float delta = clock.Restart()/10;
		
		ProcessEvents(initInfo.window, running, inputs, delta);

		counter += rotateSpeed * inputs.horizontalAxis * delta;
		lCounter += rotateSpeed * inputs.horizontalAxis2 * delta;
		cam.position = { cos(counter) * 5,1,sin(counter) * 5 };
		light.cam.position = { 3.7 * cos(lCounter),4.0f,3.7 * sin(lCounter) };




		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glViewport(0, 0, FBDEFINITION, FBDEFINITION);
		glUseProgram(graphicsInfo.lightShader);
		glBindFramebuffer(GL_FRAMEBUFFER, light.fbo.fbo);
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		glEnable(GL_CULL_FACE);
		UseCamera("viewProjection", graphicsInfo.lightShader, light.cam);
		//now draw from light's prespective
		DrawMesh(graphicsInfo.meshes[0]);
		//


		glViewport(0, 0, WIDTH, HEIGHT);
		glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
		glDisable(GL_CULL_FACE);
		glClearColor(0.35,0.4,0.7,1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(graphicsInfo.meshShader);
		glBindTexture(GL_TEXTURE_2D, graphicsInfo.paletteTexture);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, light.fbo.texture);
		UseCamera("viewProjection", graphicsInfo.meshShader, cam);
		UseCamera("lightProj", graphicsInfo.meshShader, light.cam);
		//now draw from camera's prespective
		DrawMesh(graphicsInfo.meshes[0]);

		glBindTexture(GL_TEXTURE_2D, 0);


		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUseProgram(graphicsInfo.fullScreenQuadShader);
		SetUniform(graphicsInfo.fullScreenQuadShader,"invert",inputs.spacePressed);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, fb.texture);
		DrawMesh(graphicsInfo.meshes[1]);
		glBindTexture(GL_TEXTURE_2D, 0);

		SDL_GL_SwapWindow(initInfo.window);
	}

}
