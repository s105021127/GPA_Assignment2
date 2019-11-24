#include "../Externals/Include/Include.h"

#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3

GLubyte timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;

using namespace glm;
using namespace std;

////////////
//註解:看到loadPNG要改成loadImage

const char* myScene = "breakfast_room.obj"; //這裡是load 3D檔的地方
const char* defaultSceneTex = "tiles.png";

//MVP 3矩陣
mat4 view;					// V of MVP, viewing matrix
mat4 projection;			// P of MVP, projection matrix
mat4 model;					// M of MVP, model matrix

vec3 charEye = vec3(0.3, -19.05, 0.4);
vec3 charCenter = vec3(0.3, -19.05, 0.05);
vec3 eye = charEye;
vec3 center = charCenter;
vec3 up = vec3(0.0, 1.0, 0.0);

// For meshes
struct Shape
{
	GLuint vao;
	GLuint vbo_position;
	GLuint vbo_normal;
	GLuint vbo_texcoord;
	GLuint ibo;
	int drawCount;
	int materialID;
};

struct Material
{
	GLuint diffuse_tex;
};

// Vector of meshes and materials
vector<Material> mat_vector;
vector<Shape> shape_vector;

// For original scene
GLint um4p;
GLint um4mv;
GLint tex;
GLuint program;			// shader program id (包含 vertex.vs.glsl 跟 fragment.fs.glsl)

int needFog = 1;
GLint fog1;
////////////

char** loadShaderSource(const char* file)
{
    FILE* fp = fopen(file, "rb");
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *src = new char[sz + 1];
    fread(src, sizeof(char), sz, fp);
    src[sz] = '\0';
    char **srcp = new char*[1];
    srcp[0] = src;
    return srcp;
}

void freeShaderSource(char** srcp)
{
    delete[] srcp[0];
    delete[] srcp;
}

// define a simple data structure for storing texture image raw data
typedef struct _TextureData
{
    _TextureData(void) :
        width(0),
        height(0),
        data(0)
    {
    }

    int width;
    int height;
    unsigned char* data;
} TextureData;

// load a png image and return a TextureData structure with raw data
// not limited to png format. works with any image format that is RGBA-32bit
TextureData loadImage(const char* const Filepath)
{
	TextureData texture;
	int n;
	stbi_set_flip_vertically_on_load(true);
	stbi_uc *data = stbi_load(Filepath, &texture.width, &texture.height, &n, 4);
	if (data != NULL)
	{
		texture.data = new unsigned char[texture.width * texture.height * 4 * sizeof(unsigned char)];
		memcpy(texture.data, data, texture.width * texture.height * 4 * sizeof(unsigned char));
		stbi_image_free(data);
	}
	return texture;
}

// Load a scene and get the models' verices, textures, and normals
void My_LoadScenes(const char* modelName) //這裡的sceneName就是myScene
{
	// Load the scene
	// AssImp是一個模型加載庫，它將不同格式的模型數據轉換為統一的抽象的數據類型，因而支持較多的模型文件格式。
	// 函式庫在assimp-vc140-mt.dll
	const aiScene* scene = aiImportFile(modelName, aiProcessPreset_TargetRealtime_MaxQuality); // 說明:http://assimp.sourceforge.net/lib_html/postprocess_8h.html#a64795260b95f5a4b3f3dc1be4f52e410
	if (!scene)
	{
		printf("Load model Fail!");
		exit(1); //exit(0)表示正常退出，其他表示非正常退出
	}

	// Load the textures (材質)
	printf("Load material...\n");
	for (unsigned int i = 0; i < scene->mNumMaterials; ++i) // mNumMaterials: the number of materials in the scene
	{
		aiMaterial* material = scene->mMaterials[i]; //?????
		Material mat; //?????
		aiString texturePath; //?????
		const char* def_texture = defaultSceneTex; //把"Mineways2Skfb-RGBA.png"叫進來 (多的?)

		// 老師第七章講義內容
		if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == aiReturn_SUCCESS)
		{
			// loadPNG?? .C_Str()??
			TextureData tdata = loadImage(texturePath.C_Str()); // load width, height and data from texturePath.C_Str()
			printf("%s\n", texturePath.C_Str());
			glGenTextures(1, &mat.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, mat.diffuse_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tdata.width, tdata.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tdata.data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		else
		{
			TextureData tdata = loadImage(def_texture);
			glGenTextures(1, &mat.diffuse_tex);
			glBindTexture(GL_TEXTURE_2D, mat.diffuse_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tdata.width, tdata.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tdata.data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		mat_vector.push_back(mat); //save material
	}

	// Load the models (幾何)
	printf("Load meshes...\n");


	for (unsigned int i = 0; i < scene->mNumMeshes; ++i) //mNumMeshes:幾個mesh
	{
		aiMesh* mesh = scene->mMeshes[i];
		Shape shape; //是個struct
		glGenVertexArrays(1, &shape.vao);
		glBindVertexArray(shape.vao);
		/*vector<float> positions; //頂點位置向量
		vector<float> normals; //頂點法向量
		vector<float> texcoords; //貼圖座標向量*/
		float *positions = new float[mesh->mNumVertices * 3];
		float *texcoords = new float[mesh->mNumVertices * 2];
		float *normals = new float[mesh->mNumVertices * 3];
		//positions.reserve(180);
		//normals.reserve(180);
		//texcoords.reserve(180);

		printf("%d\n",i+1);
		printf("create 3 vbos to hold data\n");
		for (int v = 0; v < mesh->mNumVertices; ++v) //mNumVertices:1個mesh有幾個vertice
		{
			printf("Start 1\n");
			
			positions[v * 3] = mesh->mVertices[v][0];
			positions[v * 3 + 1] = mesh->mVertices[v][1];
			positions[v * 3 + 2] = mesh->mVertices[v][2];
			//positions.insert(positions.end(), mesh->mVertices[v][0]);
			//positions.insert(positions.end(), mesh->mVertices[v][1]);
			//positions.insert(positions.end(), mesh->mVertices[v][2]);
			//positions.push_back(mesh->mVertices[v][1]);
			//positions.push_back(mesh->mVertices[v][2]);
			printf("Start 2\n");

			normals[v * 3] = mesh->mNormals[v][0];
			normals[v * 3 + 1] = mesh->mNormals[v][1];
			normals[v * 3 + 2] = mesh->mNormals[v][2];
			//normals.insert(normals.end(),mesh->mNormals[v][0]);
			//normals.insert(normals.end(), mesh->mNormals[v][1]);
			//normals.insert(normals.end(), mesh->mNormals[v][2]);
			//normals.push_back(mesh->mNormals[v][0]);
			//normals.push_back(mesh->mNormals[v][1]);
			//normals.push_back(mesh->mNormals[v][2]);
			printf("Start 3\n");
			
			texcoords[v * 2] = mesh->mTextureCoords[0][v][0];
			texcoords[v * 2 + 1] = mesh->mTextureCoords[0][v][1];
			cout << "Val: " << mesh->mTextureCoords[0][v][0] << endl;
			//texcoords.insert(texcoords.end(),mesh->mTextureCoords[0][v][0]);
			//texcoords.insert(texcoords.end(), mesh->mTextureCoords[0][v][1]);
			//texcoords.push_back(mesh->mTextureCoords[0][v][0]);
			//texcoords.push_back(mesh->mTextureCoords[0][v][1]);
			
		}
		printf("finish create 3 vbos to hold data\n");
		printf("create 1 iboto hold data\n");
		//vector<unsigned int> indices;
		//indices.reserve(180);
		unsigned int* indices = new unsigned int[mesh->mNumFaces * 3];

		for (unsigned int f = 0; f < mesh->mNumFaces; ++f) //儲存多邊形使用的頂點對應的頂點列表位置(p.12-30)
		{
			indices[f * 3] = mesh->mFaces[f].mIndices[0];
			indices[f * 3 + 1] = mesh->mFaces[f].mIndices[1];
			indices[f * 3 + 2] = mesh->mFaces[f].mIndices[2];
			//indices.insert(indices.end(),mesh->mFaces[f].mIndices[0]);
			//indices.insert(indices.end(), mesh->mFaces[f].mIndices[1]);
			//indices.insert(indices.end(), mesh->mFaces[f].mIndices[2]);
			//indices.push_back(mesh->mFaces[f].mIndices[0]);
			//indices.push_back(mesh->mFaces[f].mIndices[1]);
			//indices.push_back(mesh->mFaces[f].mIndices[2]);
		}
		printf("finish create 1 ibo to hold data\n");
		glGenBuffers(1, &shape.vbo_position); //頂點座標
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_position);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), positions, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0); //分配屬性資料
		glEnableVertexAttribArray(0); //啟用頂點屬性陣列
		delete[] positions;

		glGenBuffers(1, &shape.vbo_texcoord); //貼圖座標
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_texcoord);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 2 * sizeof(float), texcoords, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(1);
		delete[] texcoords;

		glGenBuffers(1, &shape.vbo_normal); //法向量座標
		glBindBuffer(GL_ARRAY_BUFFER, shape.vbo_normal);
		glBufferData(GL_ARRAY_BUFFER, mesh->mNumVertices * 3 * sizeof(float), normals, GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glEnableVertexAttribArray(2);
		delete[] normals;

		glGenBuffers(1, &shape.ibo); //頂點對應的頂點列表位置
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ibo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->mNumFaces* 3 * sizeof(unsigned int), indices, GL_STATIC_DRAW);
		delete[] indices;

		shape.materialID = mesh->mMaterialIndex;
		shape.drawCount = mesh->mNumFaces * 3;
		shape_vector.push_back(shape); //save shape(?)

		/*positions.clear();
		normals.clear();
		texcoords.clear();*/
		//indices.clear();
	}
	printf("Load shape finish\n");
	aiReleaseImport(scene);
}



void My_Init()
{
    glClearColor(0.0f, 0.6f, 0.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	program = glCreateProgram(); //創造一個空的程式物件，上面會附加許多的Shader，可以看成是一組渲染途徑(Rendering pipeline)

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER); //創建一個空的Shader object，這裡創建的Shader object是GL_VERTEX_SHADER
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	char** vertexShaderSource = loadShaderSource("vertex.vs.glsl");// Load shader file [把shader load進來!!]
	char** fragmentShaderSource = loadShaderSource("fragment.fs.glsl");
	glShaderSource(vertexShader, 1, vertexShaderSource, NULL);// Assign content of these shader files to those shaders we created before
	glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);// Free the shader file string(won't be used any more)
	freeShaderSource(fragmentShaderSource);
	glCompileShader(vertexShader);//與主程式連結完畢後，要編譯Shader object中的程式碼
	glCompileShader(fragmentShader);
	glAttachShader(program, vertexShader);//將Shader object附加到Program object上
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);//連結Program object中的Shader object，連結後的Program object就不能再改變裡面的Shader object

	um4p = glGetUniformLocation(program, "um4p");//取得預設區塊(Default block)中 Uniform block 的
	um4mv = glGetUniformLocation(program, "um4mv");
	tex = glGetUniformLocation(program, "tex");//多創建這個的用意還不明????
	//fog1 = glGetUniformLocation(program, "fog1");//多創建這個的用意還不明????

	glUseProgram(program);// Tell OpenGL to use this shader program now

	printf("Load model start!\n");
	My_LoadScenes(myScene);
	printf("Load model finish!\n");
}

void My_Display()
{
	// Clear depth and color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	/*static const GLfloat gray[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	static const GLfloat ones[] = { 1.0f };
	glClearBufferfv(GL_COLOR, 0, gray);
	glClearBufferfv(GL_DEPTH, 0, ones);*/

	mat4 Identy_Init(1.0);
	model = translate(Identy_Init, vec3(0.0, -20.0, 0.0));
	//view = lookAt(eye, center, up);
	//mat4 inv_vp_matrix = inverse(projection * view);
	view = lookAt(vec3(-10.0f, 5.0f, 0.0f), vec3(1.0f, 1.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));


	// Draw original scene
	glUseProgram(program);

	glUniformMatrix4fv(um4mv, 1, GL_FALSE, value_ptr(view * model));
	glUniformMatrix4fv(um4p, 1, GL_FALSE, value_ptr(projection));
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(tex, 0);
	//glUniform1i(fog1, needFog);

	for (int i = 0; i < shape_vector.size(); i++)
	{
		glBindVertexArray(shape_vector[i].vao);
		int materialID = shape_vector[i].materialID;
		glBindTexture(GL_TEXTURE_2D, mat_vector[materialID].diffuse_tex);
		glDrawElements(GL_TRIANGLES, shape_vector[i].drawCount, GL_UNSIGNED_INT, 0);
	}

	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
	glViewport(0, 0, width, height);
}

void My_Timer(int val)
{
	glutPostRedisplay();
	glutTimerFunc(timer_speed, My_Timer, val);
}

void My_Mouse(int button, int state, int x, int y)
{
	if(state == GLUT_DOWN)
	{
		printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
	}
	else if(state == GLUT_UP)
	{
		printf("Mouse %d is released at (%d, %d)\n", button, x, y);
	}
}

void My_Keyboard(unsigned char key, int x, int y)
{
	printf("Key %c is pressed at (%d, %d)\n", key, x, y);
}

void My_SpecialKeys(int key, int x, int y)
{
	switch(key)
	{
	case GLUT_KEY_F1:
		printf("F1 is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_PAGE_UP:
		printf("Page up is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_LEFT:
		printf("Left arrow is pressed at (%d, %d)\n", x, y);
		break;
	default:
		printf("Other special key is pressed at (%d, %d)\n", x, y);
		break;
	}
}

void My_Menu(int id)
{
	switch(id)
	{
	case MENU_TIMER_START:
		if(!timer_enabled)
		{
			timer_enabled = true;
			glutTimerFunc(timer_speed, My_Timer, 0);
		}
		break;
	case MENU_TIMER_STOP:
		timer_enabled = false;
		break;
	case MENU_EXIT:
		exit(0);
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
#ifdef __APPLE__
    // Change working directory to source code path
    chdir(__FILEPATH__("/../Assets/"));
#endif
	// Initialize GLUT and GLEW, then create a window.
	////////////////////
	glutInit(&argc, argv);
#ifdef _MSC_VER
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#else
    glutInitDisplayMode(GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(600, 600);
	glutCreateWindow("AS2_Framework"); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
#ifdef _MSC_VER
	glewInit();
#endif
    glPrintContextInfo();
	My_Init();

	// Create a menu and bind it to mouse right button.
	int menu_main = glutCreateMenu(My_Menu);
	int menu_timer = glutCreateMenu(My_Menu);

	glutSetMenu(menu_main);
	glutAddSubMenu("Timer", menu_timer);
	glutAddMenuEntry("Exit", MENU_EXIT);

	glutSetMenu(menu_timer);
	glutAddMenuEntry("Start", MENU_TIMER_START);
	glutAddMenuEntry("Stop", MENU_TIMER_STOP);

	glutSetMenu(menu_main);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	// Register GLUT callback functions.
	glutDisplayFunc(My_Display);
	glutReshapeFunc(My_Reshape);
	glutMouseFunc(My_Mouse);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutTimerFunc(timer_speed, My_Timer, 0); 

	// Enter main event loop.
	glutMainLoop();

	return 0;
}
