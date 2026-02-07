#include <windows.h>
#include <gl/gl.h>
#include <gl/glu.h>
#include <vector>
#include <cmath>
#include <string>
#include <iostream>
#include <gdiplus.h>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "gdiplus.lib")
using namespace std;
using namespace Gdiplus;

// 物理引擎参数
#define EPS 2
#define PDCK 2
#define pi 3.14159265358979323846
#define TABLE_WIDTH 1280.0f
#define TABLE_HEIGHT 640.0f
#define TABLE_THICKNESS 50.0f
#define BALL_RADIUS 20.0f
#define BALL_COUNT 16
#define WHITE_BALL_INDEX 0
#define FRICTION 0.996f
#define HOLE_RADIUS 35.0f
#define POWER 0.3f  // 调整力度系数适配新瞄准方式
#define MIN_SPEED 1
#define BALL_SEGMENTS 32  // 球体分段数
#define AIM_LINE_WIDTH 2.0f    // 瞄准线宽度
#define AIM_LINE_LENGTH 1000.0f // 瞄准线长度

// 游戏手柄参数 - 统一基于窗口右下角（Y轴从上到下）
#define JOYSTICK_CENTER_X 1100.0f  // 手柄圆心X（窗口右下方）
#define JOYSTICK_CENTER_Y 620.0f   // 手柄圆心Y（窗口右下方）
#define JOYSTICK_BIG_RADIUS 80.0f  // 大圆圆半径
#define JOYSTICK_SMALL_RADIUS 30.0f// 小圆圆半径
#define JOYSTICK_BIG_COLOR 0xCCCCCC// 浅灰色
#define JOYSTICK_SMALL_COLOR 0x666666// 深灰色

// 自动视角参数
#define CAMERA_SMOOTH_SPEED 0.05f  // 摄像机平滑移动速度
#define CAMERA_HEIGHT_FACTOR 1.2f  // 摄像机高度系数（基于场景宽度）
#define CAMERA_PADDING 100.0f      // 视野边缘额外padding
#define MIN_CAMERA_DISTANCE double(400.0f) // 最小摄像机距离
#define MAX_CAMERA_DISTANCE double(1000.0f)// 最大摄像机距离

// 颜色定义 (RGB 0-1范围) - 重命名为BallColor避免和Gdiplus::Color冲突
struct BallColor {
    double r, g, b;
};

const BallColor WHITE_COLOR = {1.0f, 1.0f, 1.0f};
const BallColor RED_COLOR = {1.0f, 0.0f, 0.0f};
const BallColor YELLOW_COLOR = {1.0f, 1.0f, 0.0f};
const BallColor GREEN_COLOR = {0.0f, 1.0f, 0.0f};
const BallColor BLUE_COLOR = {0.0f, 0.0f, 1.0f};
const BallColor PURPLE_COLOR = {0.5f, 0.0f, 0.5f};
const BallColor ORANGE_COLOR = {1.0f, 0.65f, 0.0f};
const BallColor BROWN_COLOR = {0.65f, 0.16f, 0.16f};
const BallColor BLACK_COLOR = {0.0f, 0.0f, 0.0f};
const BallColor TABLE_COLOR = {0.2f, 0.7f, 0.2f};  // 桌布绿色
const BallColor BORDER_COLOR = {0.54f, 0.27f, 0.07f}; // 边框棕色

// 全局变量 - 贴图相关
GLuint whiteBallTexture = 0;
bool textureLoaded = false; // 标记纹理是否成功加载
Gdiplus::GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;

// 向量结构体
struct vec2
{
    double x, y;
};

struct vec3
{
    double x, y, z;
    vec3 operator+(const vec3& v) const { return {x+v.x, y+v.y, z+v.z}; }
    vec3 operator-(const vec3& v) const { return {x-v.x, y-v.y, z-v.z}; }
    vec3 operator*(double s) const { return {x*s, y*s, z*s}; }
    vec3 operator/(double s) const { return {x/s, y/s, z/s}; }
    
    // 插值函数
    vec3 lerp(const vec3& target, double t) const {
        return {
            x + (target.x - x) * t,
            y + (target.y - y) * t,
            z + (target.z - z) * t
        };
    }
};

// 球结构体
struct ball
{
    vec2 pos;               
    vec2 speed;             
    double r;               
    BallColor color;        // 改为BallColor
    bool isfancy;           
    bool isPotted;          

    vec3 Get3DPos() const {
        return { (double)pos.x, (double)r, (double)pos.y };
    }
    
    double dis(ball b)      
    {
        return sqrt((pos.x - b.pos.x) * (pos.x - b.pos.x) + (pos.y - b.pos.y) * (pos.y - b.pos.y));
    }
    
    bool collision(ball b)  
    {
        if (dis(b) <= r + b.r + EPS)
        {
            ball nxta; nxta.pos = { pos.x + speed.x * PDCK, pos.y + speed.y * PDCK };
            ball nxtb; nxtb.pos = { b.pos.x + b.speed.x * PDCK, b.pos.y + b.speed.y * PDCK };
            if (nxta.dis(nxtb) <= r + b.r + EPS)
                return true;
            else return false;
        }
        return false;
    }
    
    void makespeed(double direction, double extent)  
    {
        speed.x += sin(direction / 180.0 * pi) * extent;
        speed.y += cos(direction / 180.0 * pi) * extent;
    }
    
    void makespeedEx(vec2 vec, double extent)        
    {
        double length = sqrt(vec.x * vec.x + vec.y * vec.y);
        if (length > 0) {
            vec.x *= extent / length;
            vec.y *= extent / length;
            speed.x += vec.x;
            speed.y += vec.y;
        }
    }
    
    void move()                                     
    {
        pos.x += speed.x;
        pos.y += speed.y;
        
        speed.x *= FRICTION;
        speed.y *= FRICTION;
        
        if (fabs(speed.x) < MIN_SPEED && fabs(speed.y) < MIN_SPEED) {
            speed.x = 0;
            speed.y = 0;
        }
    }
};

// 游戏状态枚举
enum GameState {
    AIMING,      
    PLAYING,     
    GAME_OVER    
};

// 全局变量
HWND g_hWnd = NULL;
HDC g_hDC = NULL;
HGLRC g_hRC = NULL;
std::vector<ball> balls;
GameState gameState = AIMING;
bool isAiming = false;
vec2 joystickSmallPos = {JOYSTICK_CENTER_X, JOYSTICK_CENTER_Y}; // 手柄小圆初始位置和大圆一致
int windowWidth = 1280;
int windowHeight = 720;
bool gameRunning = true;

// 摄像机相关全局变量
vec3 currentCameraPos = {TABLE_WIDTH/2 - 200, 600, TABLE_HEIGHT/2 - 400}; // 当前摄像机位置
vec3 targetCameraPos = {TABLE_WIDTH/2 - 200, 600, TABLE_HEIGHT/2 - 400};  // 目标摄像机位置
vec3 lookAtPos = {TABLE_WIDTH/2, 0, TABLE_HEIGHT/2};                     // 观察目标点

// 函数声明
void ResizeGLScene(int width, int height);
void DrawAimLine3D();
bool LoadTextureFromBMP(const wchar_t* filename, GLuint& textureID);
void DrawJoystick();
void DrawTexturedSphere(double radius, GLuint textureID);
void CalculateTargetCameraPosition();
void UpdateCameraSmoothly();

// 计算目标摄像机位置（容纳所有可见球）
void CalculateTargetCameraPosition() {
    if (balls.empty()) {
        targetCameraPos = {TABLE_WIDTH/2 - 200, 600, TABLE_HEIGHT/2 - 400};
        lookAtPos = {TABLE_WIDTH/2, 0, TABLE_HEIGHT/2};
        return;
    }
    
    // 1. 找到所有未入洞球的边界
    double minX = TABLE_WIDTH, maxX = 0;
    double minZ = TABLE_HEIGHT, maxZ = 0;
    int visibleBalls = 0;
    
    for (const auto& b : balls) {
        if (b.isPotted) continue;
        
        minX = min(minX, b.pos.x - CAMERA_PADDING);
        maxX = max(maxX, b.pos.x + CAMERA_PADDING);
        minZ = min(minZ, b.pos.y - CAMERA_PADDING);
        maxZ = max(maxZ, b.pos.y + CAMERA_PADDING);
        visibleBalls++;
    }
    
    // 如果没有可见球，使用默认位置
    if (visibleBalls == 0) {
        minX = 0; maxX = TABLE_WIDTH;
        minZ = 0; maxZ = TABLE_HEIGHT;
    }
    
    // 2. 计算场景中心点
    double centerX = (minX + maxX) / 2.0;
    double centerZ = (minZ + maxZ) / 2.0;
    lookAtPos = {centerX, 0, centerZ};
    
    // 3. 计算场景宽度和高度
    double sceneWidth = maxX - minX;
    double sceneHeight = maxZ - minZ;
    double maxDimension = max(sceneWidth, sceneHeight);
    
    // 4. 计算合适的摄像机距离（基于透视投影）
    double fovRadians = 45.0 * pi / 180.0;
    double cameraDistance = (maxDimension / 2.0) / tan(fovRadians / 2.0);
    
    // 限制摄像机距离范围
    cameraDistance = max(MIN_CAMERA_DISTANCE, min(MAX_CAMERA_DISTANCE, cameraDistance));
    
    // 5. 计算摄像机高度
    double cameraHeight = cameraDistance * 0.7; // 45度俯视角
    cameraHeight = max(cameraHeight, maxDimension * CAMERA_HEIGHT_FACTOR);
    
    // 6. 计算目标摄像机位置（从后上方俯视中心点）
    targetCameraPos = {
        centerX - cameraDistance * 0.5,  // 稍微偏左后
        cameraHeight,                    // 高度
        centerZ - cameraDistance * 0.5   // 稍微偏下后
    };
}

// 平滑更新摄像机位置
void UpdateCameraSmoothly() {
    // 使用线性插值平滑移动摄像机
    currentCameraPos = currentCameraPos.lerp(targetCameraPos, CAMERA_SMOOTH_SPEED);
    
    // 当距离足够近时，直接设置为目标位置（避免无限逼近）
    double dx = currentCameraPos.x - targetCameraPos.x;
    double dy = currentCameraPos.y - targetCameraPos.y;
    double dz = currentCameraPos.z - targetCameraPos.z;
    double distance = sqrt(dx*dx + dy*dy + dz*dz);
    
    if (distance < 0.1) {
        currentCameraPos = targetCameraPos;
    }
}

// 加载BMP贴图为OpenGL纹理（增强错误处理）
bool LoadTextureFromBMP(const wchar_t* filename, GLuint& textureID) {
    // 先删除旧纹理（避免内存泄漏）
    if (glIsTexture(textureID)) {
        glDeleteTextures(1, &textureID);
    }
    
    // 创建新纹理ID
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // 设置纹理参数（即使加载失败也有默认参数）
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // 尝试加载位图
    Bitmap bitmap(filename);
    if (bitmap.GetWidth() == 0 || bitmap.GetHeight() == 0) {
        MessageBoxW(NULL, L"加载白球贴图a.bmp失败，将使用白色替代", L"警告", MB_OK);
        return false;
    }

    BitmapData bitmapData;
    Rect rect(0, 0, bitmap.GetWidth(), bitmap.GetHeight());
    if (bitmap.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) != Ok) {
        MessageBoxW(NULL, L"锁定贴图数据失败，将使用白色替代", L"警告", MB_OK);
        return false;
    }

    // 上传纹理数据
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bitmap.GetWidth(), bitmap.GetHeight(), 
                 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, bitmapData.Scan0);
    bitmap.UnlockBits(&bitmapData);
    
    // 验证纹理是否有效
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        MessageBoxW(NULL, L"纹理上传失败，将使用白色替代", L"警告", MB_OK);
        return false;
    }
    
    return true;
}

// OpenGL初始化函数
bool InitOpenGL(HWND hWnd)
{
    // 初始化GDI+
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // 获取设备上下文
    g_hDC = GetDC(hWnd);
    
    // 设置像素格式
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,      // 颜色深度
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        32,      // 深度缓冲区
        0,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };
    
    int pixelFormat = ChoosePixelFormat(g_hDC, &pfd);
    if (!pixelFormat) return false;
    
    if (!SetPixelFormat(g_hDC, pixelFormat, &pfd)) return false;
    
    // 创建渲染上下文
    g_hRC = wglCreateContext(g_hDC);
    if (!g_hRC) return false;
    
    if (!wglMakeCurrent(g_hDC, g_hRC)) return false;
    
    // 初始化OpenGL状态
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 背景深灰色
    glEnable(GL_DEPTH_TEST);              // 启用深度测试
    glEnable(GL_COLOR_MATERIAL);
    glShadeModel(GL_SMOOTH);              // 平滑着色
    glEnable(GL_LIGHTING);                // 启用光照（让球体有3D效果）
    glEnable(GL_LIGHT0);                  // 启用0号光源
    glEnable(GL_TEXTURE_2D);              // 始终启用纹理（修复白球黑色问题）
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_LINE_SMOOTH);    
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glLineWidth(AIM_LINE_WIDTH);  // 设置线宽

    // 设置光源参数
    GLfloat lightPos[] = {0.0f, 1000.0f, 500.0f, 1.0f};
    GLfloat lightAmbient[] = {0.3f, 0.3f, 0.3f, 2.0f};
    GLfloat lightDiffuse[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    
    // 加载白球贴图（提前加载，确保初始状态可用）
    textureLoaded = LoadTextureFromBMP(L"1.bmp", whiteBallTexture);
    
    // 设置投影和视角
    ResizeGLScene(windowWidth, windowHeight);
    
    return true;
}

// 调整OpenGL视口
void ResizeGLScene(int width, int height)
{
    if (height == 0) height = 1;
    
    glViewport(0, 0, width, height);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    // 设置透视投影（GLU内置函数，无需自己算矩阵）
    gluPerspective(45.0f, (double)width/height, 10.0f, 2000.0f);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// 绘制带纹理的球体（增强鲁棒性）
void DrawTexturedSphere(double radius, GLuint textureID) {
    // 绑定纹理（即使纹理无效也不会黑屏）
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    GLUquadric* quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluQuadricTexture(quad, GL_TRUE); // 强制启用纹理坐标
    gluSphere(quad, radius, BALL_SEGMENTS, BALL_SEGMENTS);
    gluDeleteQuadric(quad);
    
    // 恢复默认纹理（避免影响其他绘制）
    glBindTexture(GL_TEXTURE_2D, 0);
}

// 绘制普通球体
void DrawSphere(double radius, const BallColor& color) // 改为BallColor
{
    glColor3d(color.r, color.g, color.b);
    GLUquadric* quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluSphere(quad, radius, BALL_SEGMENTS, BALL_SEGMENTS);
    gluDeleteQuadric(quad);
}

// 绘制台球（修复白球纹理始终显示）
void DrawBall(const ball& b)
{
    if (b.isPotted) return;
    
    glPushMatrix(); // 保存当前矩阵
    
    // 移动到球的3D位置
    vec3 pos = b.Get3DPos();
    glTranslated(pos.x, pos.y, pos.z);
    
    // 绘制白球（优先用纹理，失败则用白色）
    if (&b == &balls[WHITE_BALL_INDEX]) {
        glColor3d(1.0f, 1.0f, 1.0f); // 白色底色（纹理透明时兜底）
        if (textureLoaded) {
            DrawTexturedSphere(b.r, whiteBallTexture);
        } else {
            DrawSphere(b.r, WHITE_COLOR); // 纹理加载失败时绘制纯色白球
        }
    } else {
        // 绘制其他球
        DrawSphere(b.r, b.color);
        
        // 绘制花色球内部的白球
        if (b.isfancy) {
            glPushMatrix();
            glScaled(0.4f, 0.4f, 0.4f); // 缩小到40%
            DrawSphere(b.r, WHITE_COLOR);
            glPopMatrix();
        }
    }
    
    glPopMatrix(); // 恢复矩阵
}

// 绘制台球桌
void DrawTable()
{
    glPushMatrix();
    
    // 绘制桌面（绿色）
    glColor3d(TABLE_COLOR.r, TABLE_COLOR.g, TABLE_COLOR.b);
    glBegin(GL_QUADS);
    // 桌面顶面
    glVertex3d(0, 0, 0);
    glVertex3d(TABLE_WIDTH, 0, 0);
    glVertex3d(TABLE_WIDTH, 0, TABLE_HEIGHT);
    glVertex3d(0, 0, TABLE_HEIGHT);
    glEnd();
    
    // 绘制边框（棕色）
    glColor3d(BORDER_COLOR.r, BORDER_COLOR.g, BORDER_COLOR.b);
    
    // 前边框（z=0）
    glBegin(GL_QUADS);
    glVertex3d(-20, 0, -20);
    glVertex3d(TABLE_WIDTH + 20, 0, -20);
    glVertex3d(TABLE_WIDTH + 20, TABLE_THICKNESS, -20);
    glVertex3d(-20, TABLE_THICKNESS, -20);
    glEnd();
    
    // 后边框（z=TABLE_HEIGHT）
    glBegin(GL_QUADS);
    glVertex3d(-20, 0, TABLE_HEIGHT + 20);
    glVertex3d(TABLE_WIDTH + 20, 0, TABLE_HEIGHT + 20);
    glVertex3d(TABLE_WIDTH + 20, TABLE_THICKNESS, TABLE_HEIGHT + 20);
    glVertex3d(-20, TABLE_THICKNESS, TABLE_HEIGHT + 20);
    glEnd();
    
    // 左边框（x=0）
    glBegin(GL_QUADS);
    glVertex3d(-20, 0, -20);
    glVertex3d(-20, 0, TABLE_HEIGHT + 20);
    glVertex3d(-20, TABLE_THICKNESS, TABLE_HEIGHT + 20);
    glVertex3d(-20, TABLE_THICKNESS, -20);
    glEnd();
    
    // 右边框（x=TABLE_WIDTH）
    glBegin(GL_QUADS);
    glVertex3d(TABLE_WIDTH + 20, 0, -20);
    glVertex3d(TABLE_WIDTH + 20, 0, TABLE_HEIGHT + 20);
    glVertex3d(TABLE_WIDTH + 20, TABLE_THICKNESS, TABLE_HEIGHT + 20);
    glVertex3d(TABLE_WIDTH + 20, TABLE_THICKNESS, -20);
    glEnd();
    
    // 绘制球洞（黑色）
    glColor3d(BLACK_COLOR.r, BLACK_COLOR.g, BLACK_COLOR.b);
    GLUquadric* quad = gluNewQuadric();
    gluQuadricNormals(quad, GLU_SMOOTH);
    
    // 六个球洞位置
    vec2 holes[] = {
        {0, 0}, {TABLE_WIDTH/2, 0}, {TABLE_WIDTH, 0},
        {0, TABLE_HEIGHT}, {TABLE_WIDTH/2, TABLE_HEIGHT}, {TABLE_WIDTH, TABLE_HEIGHT}
    };
    
    for (auto& hole : holes) {
        glPushMatrix();
        glTranslated((double)hole.x, 0, (double)hole.y);
        glScaled(1.0f, 0.5f, 1.0f);
        gluSphere(quad, HOLE_RADIUS, 16, 16);
        glPopMatrix();
    }
    
    gluDeleteQuadric(quad);
    glPopMatrix();
}

// 修复后的2D游戏手柄绘制函数（坐标+判定双修复）
void DrawJoystick() {
    // 1. 保存当前所有矩阵状态
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    
    // 2. 切换到2D正交投影
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth, windowHeight, 0, -1, 1); // Y轴从上到下
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // 3. 关闭干扰2D绘制的状态
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    
    // 4. 绘制大圆环（右下方，和小圆同中心点）
    glColor3ub((JOYSTICK_BIG_COLOR >> 16) & 0xFF, 
               (JOYSTICK_BIG_COLOR >> 8) & 0xFF, 
               JOYSTICK_BIG_COLOR & 0xFF);
    glPushMatrix();
    // 平移到大圆中心点（修复左上角问题）
    glTranslatef((GLfloat)JOYSTICK_CENTER_X, (GLfloat)JOYSTICK_CENTER_Y, 0.0f);
    GLUquadric* quad = gluNewQuadric();
    gluDisk(quad, JOYSTICK_BIG_RADIUS - 5, JOYSTICK_BIG_RADIUS, 32, 1);
    gluDeleteQuadric(quad);
    glPopMatrix();
    
    // 5. 绘制小圆（深灰色）
    glColor3ub((JOYSTICK_SMALL_COLOR >> 16) & 0xFF, 
               (JOYSTICK_SMALL_COLOR >> 8) & 0xFF, 
               JOYSTICK_SMALL_COLOR & 0xFF);
    glPushMatrix();
    glTranslatef((GLfloat)joystickSmallPos.x, (GLfloat)joystickSmallPos.y, 0.0f);
    quad = gluNewQuadric();
    gluDisk(quad, 0, JOYSTICK_SMALL_RADIUS, 32, 1);
    gluDeleteQuadric(quad);
    glPopMatrix();
    
    // 6. 恢复所有矩阵和状态
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    
    glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

void DrawAimLine3D() {
    if (!isAiming || balls.empty()) return;
    
    // 计算瞄准方向（从手柄圆心指向小圆）
    double dirX = JOYSTICK_CENTER_X - joystickSmallPos.x;
    double dirY = JOYSTICK_CENTER_Y - joystickSmallPos.y;
    double dirLen = sqrt(dirX*dirX + dirY*dirY);
    
    // 方向归一化
    if (dirLen < 1.0) {  
        dirX = 0;
        dirY = -1;
        dirLen = 1;
    } else {
        dirX /= dirLen;
        dirY /= dirLen;
    }
    
    // 获取白球位置（3D）
    ball& whiteBall = balls[0];
    vec3 startPos = {
        (double)whiteBall.pos.x,
        (double)whiteBall.r + 1.0f,
        (double)whiteBall.pos.y
    };
    
    // 计算瞄准线终点（3D）
    vec3 endPos = {
        startPos.x + (double)(dirX * AIM_LINE_LENGTH),
        startPos.y,
        startPos.z + (double)(dirY * AIM_LINE_LENGTH)
    };
    
    // 绘制3D瞄准线
    glPushMatrix();
    glDisable(GL_LIGHTING);
    glColor3d(1.0f, 1.0f, 0.0f);
    
    // 主瞄准线
    glBegin(GL_LINES);
    glVertex3d(startPos.x, startPos.y, startPos.z);
    glVertex3d(endPos.x, endPos.y, endPos.z);
    glEnd();
    
    // 辅助虚线
    glColor3d(1.0f, 0.5f, 0.0f);
    glLineStipple(5, 0xEEEE);
    glEnable(GL_LINE_STIPPLE);
    
    for (int i = -2; i <= 2; i += 2) {
        vec3 offsetStart = {
            startPos.x + (double)i * 5.0f,
            startPos.y - 1.0f,
            startPos.z
        };
        vec3 offsetEnd = {
            endPos.x + (double)i * 15.0f,
            endPos.y - 1.0f,
            endPos.z
        };
        
        glBegin(GL_LINES);
        glVertex3d(offsetStart.x, offsetStart.y, offsetStart.z);
        glVertex3d(offsetEnd.x, offsetEnd.y, offsetEnd.z);
        glEnd();
    }
    
    glDisable(GL_LINE_STIPPLE);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

// 渲染场景
void RenderScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    // 计算目标摄像机位置并平滑更新
    CalculateTargetCameraPosition();
    UpdateCameraSmoothly();
    
    // 使用动态摄像机位置（替换原来的固定视角）
    gluLookAt(
        currentCameraPos.x, currentCameraPos.y, currentCameraPos.z,
        lookAtPos.x, lookAtPos.y, lookAtPos.z,
        0, 1, 0  // 向上向量
    );
    
    // 绘制3D场景
    DrawTable();
    DrawAimLine3D();
    for (const auto& b : balls) {
        DrawBall(b);
    }
    
    // 绘制2D手柄
    DrawJoystick();
    
    SwapBuffers(g_hDC);
}

// 释放OpenGL资源
void CleanupOpenGL()
{
    if (glIsTexture(whiteBallTexture)) {
        glDeleteTextures(1, &whiteBallTexture);
    }
    
    GdiplusShutdown(gdiplusToken);
    
    if (g_hRC) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_hRC);
    }
    if (g_hDC) {
        ReleaseDC(g_hWnd, g_hDC);
    }
}

// 初始化球
void InitBalls()
{
    balls.clear();
    
    // 白球
    ball whiteBall;
    whiteBall.pos = { TABLE_WIDTH * 0.75, TABLE_HEIGHT * 0.5 };
    whiteBall.speed = { 0, 0 };
    whiteBall.r = BALL_RADIUS;
    whiteBall.color = WHITE_COLOR;
    whiteBall.isfancy = false;
    whiteBall.isPotted = false;
    balls.push_back(whiteBall);
    
    // 其他球
    BallColor colors[] = {
        RED_COLOR, YELLOW_COLOR, GREEN_COLOR, BLUE_COLOR,
        PURPLE_COLOR, ORANGE_COLOR, BROWN_COLOR, BLACK_COLOR
    };
    
    int rows = 5;
    int count = 0;
    double startX = TABLE_WIDTH * 0.25;
    double startY = TABLE_HEIGHT * 0.5;
    double offsetX = BALL_RADIUS * 1.8;
    double offsetY = BALL_RADIUS * 1.5;
    
    for (int row = 0; row < rows; row++)
    {
        for (int col = 0; col <= row; col++)
        {
            if (count >= 15) break;
            
            ball b;
            b.pos = { 
                startX + (rows - 1 - row) * offsetX, 
                startY - (row * offsetY / 2) + col * offsetY 
            };
            b.speed = { 0, 0 };
            b.r = BALL_RADIUS;
            
            b.color = colors[count % 8];
            b.isfancy = (count % 2 == 1);
            b.isPotted = false;
            
            balls.push_back(b);
            count++;
        }
    }
}

// 检查球是否入洞
void CheckPottedBalls()
{
    vec2 holes[] = {
        { 0, 0 }, { TABLE_WIDTH / 2, 0 }, { TABLE_WIDTH, 0 },
        { 0, TABLE_HEIGHT }, { TABLE_WIDTH / 2, TABLE_HEIGHT }, { TABLE_WIDTH, TABLE_HEIGHT }
    };
    
    for (auto& ball : balls)
    {
        if (ball.isPotted) continue;
        
        vec2 ballPos = { ball.pos.x, ball.pos.y };
        
        for (auto& hole : holes)
        {
            double dx = ballPos.x - hole.x;
            double dy = ballPos.y - hole.y;
            double distance = sqrt(dx * dx + dy * dy);
            
            if (distance < HOLE_RADIUS - 5)
            {
                ball.isPotted = true;
                
                if (&ball == &balls[WHITE_BALL_INDEX])
                {
                    ball.pos = { TABLE_WIDTH * 0.75, TABLE_HEIGHT * 0.5 };
                    ball.speed = { 0, 0 };
                    ball.isPotted = false;
                }
                
                break;
            }
        }
    }
}

// 检查所有球是否停止运动
bool AreAllBallsStopped()
{
    for (auto& ball : balls)
    {
        if (ball.isPotted) continue;
        
        if (fabs(ball.speed.x) > MIN_SPEED || fabs(ball.speed.y) > MIN_SPEED)
            return false;
    }
    return true;
}

// 限制手柄小圆在大圆内
void ClampJoystickSmallPos() {
    double dx = joystickSmallPos.x - JOYSTICK_CENTER_X;
    double dy = joystickSmallPos.y - JOYSTICK_CENTER_Y;
    double dist = sqrt(dx*dx + dy*dy);
    
    if (dist > JOYSTICK_BIG_RADIUS - JOYSTICK_SMALL_RADIUS) {
        double ratio = (JOYSTICK_BIG_RADIUS - JOYSTICK_SMALL_RADIUS) / dist;
        joystickSmallPos.x = JOYSTICK_CENTER_X + dx * ratio;
        joystickSmallPos.y = JOYSTICK_CENTER_Y + dy * ratio;
    }
}

// 物理引擎
void UpdatePhysics()
{
    if (gameState != PLAYING) return;
    
    int numBalls = balls.size();
    
    // 球与球碰撞
    for (int i = 0; i < numBalls; ++i)
    {
        if (balls[i].isPotted) continue;
        
        for (int j = i + 1; j < numBalls; ++j)
        {
            if (balls[j].isPotted) continue;
            
            if (balls[i].collision(balls[j]))
            {
                vec2 normal = { balls[j].pos.x - balls[i].pos.x, balls[j].pos.y - balls[i].pos.y };
                double dist = sqrt(normal.x * normal.x + normal.y * normal.y);
                if (dist > 0) {
                    normal.x /= dist;
                    normal.y /= dist;
                }
                
                double v1n = balls[i].speed.x * normal.x + balls[i].speed.y * normal.y;
                double v2n = balls[j].speed.x * normal.x + balls[j].speed.y * normal.y;
                double v1t = balls[i].speed.x * (-normal.y) + balls[i].speed.y * normal.x;
                double v2t = balls[j].speed.x * (-normal.y) + balls[j].speed.y * normal.x;
                
                balls[i].speed.x = v2n * normal.x + v1t * (-normal.y);
                balls[i].speed.y = v2n * normal.y + v1t * normal.x;
                balls[j].speed.x = v1n * normal.x + v2t * (-normal.y);
                balls[j].speed.y = v1n * normal.y + v2t * normal.x;
                
                double overlap = balls[i].r + balls[j].r - dist;
                if (overlap > 0) {
                    balls[i].pos.x -= normal.x * overlap * 0.5;
                    balls[i].pos.y -= normal.y * overlap * 0.5;
                    balls[j].pos.x += normal.x * overlap * 0.5;
                    balls[j].pos.y += normal.y * overlap * 0.5;
                }
            }
        }
        
        // 边界碰撞
        double leftBound = balls[i].r;
        double rightBound = TABLE_WIDTH - balls[i].r;
        double topBound = balls[i].r;
        double bottomBound = TABLE_HEIGHT - balls[i].r;
        
        if (balls[i].pos.x < leftBound)
        {
            balls[i].pos.x = leftBound;
            balls[i].speed.x = -balls[i].speed.x * 0.8;
        }
        else if (balls[i].pos.x > rightBound)
        {
            balls[i].pos.x = rightBound;
            balls[i].speed.x = -balls[i].speed.x * 0.8;
        }
        
        if (balls[i].pos.y < topBound)
        {
            balls[i].pos.y = topBound;
            balls[i].speed.y = -balls[i].speed.y * 0.8;
        }
        else if (balls[i].pos.y > bottomBound)
        {
            balls[i].pos.y = bottomBound;
            balls[i].speed.y = -balls[i].speed.y * 0.8;
        }
    }
    
    // 移动球
    for (auto& ball : balls)
    {
        if (!ball.isPotted)
            ball.move();
    }
    
    // 检查入洞
    CheckPottedBalls();
    
    // 检查停止
    if (AreAllBallsStopped())
    {
        gameState = AIMING;
        joystickSmallPos = {JOYSTICK_CENTER_X, JOYSTICK_CENTER_Y};
    }
}

// 窗口消息处理（修复判定中心点偏移）
LRESULT CALLBACK MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_LBUTTONDOWN:
        {
            if (gameState != AIMING) break;
            
            // 鼠标坐标（原生：X左→右，Y上→下）
            int mouseX = LOWORD(lParam);
            int mouseY = HIWORD(lParam);
            
            // 计算鼠标到手柄小圆的距离（无需反转Y轴，因为判定基于原生窗口坐标）
            double dx = mouseX - joystickSmallPos.x;
            double dy = mouseY - joystickSmallPos.y;
            if (sqrt(dx*dx + dy*dy) <= JOYSTICK_SMALL_RADIUS) {
                isAiming = true;
            }
            break;
        }
        case WM_MOUSEMOVE:
        {
            if (isAiming) {
                // 直接使用原生鼠标坐标（修复Y轴反转导致的偏移）
                joystickSmallPos.x = LOWORD(lParam);
                joystickSmallPos.y = HIWORD(lParam);
                ClampJoystickSmallPos();
            }
            break;
        }
        case WM_LBUTTONUP:
        {
            if (isAiming && gameState == AIMING)
            {
                isAiming = false;
                // 计算方向（基于原生坐标，中心点为JOYSTICK_CENTER）
                double dirX = JOYSTICK_CENTER_X - joystickSmallPos.x;
                double dirY = JOYSTICK_CENTER_Y - joystickSmallPos.y;
                double length = sqrt(dirX*dirX + dirY*dirY);
                
                if (length > 0)
                {
                    balls[WHITE_BALL_INDEX].makespeedEx({dirX, dirY}, length * POWER);
                    gameState = PLAYING;
                }
            }
            break;
        }
        case WM_SIZE:
        {
            windowWidth = LOWORD(lParam);
            windowHeight = HIWORD(lParam);
            ResizeGLScene(windowWidth, windowHeight);
            break;
        }
        case WM_DESTROY:
        {
            gameRunning = false;
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// 主函数
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd)
{
    HICON hico=(HICON)LoadImage(NULL,"Icon.ico",IMAGE_ICON,32,32,LR_LOADFROMFILE|LR_DEFAULTSIZE);
    HICON hicosmall=(HICON)LoadImage(NULL,"Icon.ico",IMAGE_ICON,16,16,LR_LOADFROMFILE|LR_DEFAULTSIZE);
    // 注册窗口类
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = MsgProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInst;
    wc.hIcon = hico;
    wc.hIconSm = hicosmall;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "OpenGLBilliards";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBoxW(NULL, L"注册窗口类失败", L"错误", MB_OK);
        return 0;
    }

    // 创建窗口
    g_hWnd = CreateWindowW(L"OpenGLBilliards", L"OpenGL 3D台球游戏",
                          WS_OVERLAPPEDWINDOW, 100, 100, windowWidth, windowHeight,
                          NULL, NULL, hInst, NULL);
    SendMessage(g_hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hicosmall);
    if (!g_hWnd) {
        MessageBoxW(NULL, L"创建窗口失败", L"错误", MB_OK);
        return 0;
    }

    // 初始化OpenGL
    if (!InitOpenGL(g_hWnd))
    {
        MessageBoxW(NULL, L"初始化OpenGL失败", L"错误", MB_OK);
        CleanupOpenGL();
        UnregisterClassW(L"OpenGLBilliards", hInst);
        return 0;
    }

    // 初始化球
    InitBalls();

    // 显示窗口
    ShowWindow(g_hWnd, nShowCmd);
    UpdateWindow(g_hWnd);

    // 消息循环
    MSG msg = { 0 };
    while (gameRunning && msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            UpdatePhysics();
            RenderScene();
            Sleep(1);
        }
    }

    // 清理资源
    CleanupOpenGL();
    DeleteObject(hico);
    DeleteObject(hicosmall);
    UnregisterClassW(L"OpenGLBilliards", hInst);
    return 0;
}