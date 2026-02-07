#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include <cmath>
#include <string>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

// 物理引擎参数
#define EPS 2
#define PDCK 2
#define pi (3.1415926535897932384626433832795)
#define TABLE_WIDTH 1280
#define TABLE_HEIGHT 640
#define BALL_RADIUS 20
#define BALL_COUNT 16
#define WHITE_BALL_INDEX 0
#define FRICTION 0.996f  //0.98f -> 0.996f
#define HOLE_RADIUS 35
#define POWER 3.0f        //1.0f -> 3.0f
#define MIN_SPEED 1
/*修改:第471行 调整球的布局，原:row * offsetX,*/
// 颜色定义
D3DCOLOR WHITE_COLOR = D3DCOLOR_XRGB(255, 255, 255);
D3DCOLOR RED_COLOR = D3DCOLOR_XRGB(255, 0, 0);
D3DCOLOR YELLOW_COLOR = D3DCOLOR_XRGB(255, 255, 0);
D3DCOLOR GREEN_COLOR = D3DCOLOR_XRGB(0, 255, 0);
D3DCOLOR BLUE_COLOR = D3DCOLOR_XRGB(0, 0, 255);
D3DCOLOR PURPLE_COLOR = D3DCOLOR_XRGB(128, 0, 128);
D3DCOLOR ORANGE_COLOR = D3DCOLOR_XRGB(255, 165, 0);
D3DCOLOR BROWN_COLOR = D3DCOLOR_XRGB(165, 42, 42);
D3DCOLOR BLACK_COLOR = D3DCOLOR_XRGB(0, 0, 0);
D3DCOLOR TABLE_COLOR = D3DCOLOR_XRGB(50, 100, 50);
D3DCOLOR BORDER_COLOR = D3DCOLOR_XRGB(139, 69, 19);

// 向量结构体
struct vec2
{
    double x, y;
};

// 球结构体
struct ball
{
    vec2 pos;               // 坐标
    vec2 speed;             // 速度
    double r;               // 半径
    D3DCOLOR color;         // 颜色
    bool isfancy;           // 是否是花色球
    bool isPotted;          // 是否已入洞
    
    // 计算到另一个球的距离
    double dis(ball b)      
    {
        return sqrt((pos.x - b.pos.x) * (pos.x - b.pos.x) + (pos.y - b.pos.y) * (pos.y - b.pos.y));
    }
    
    // 碰撞检测
    bool collision(ball b)  
    {
        if (dis(b) <= r + b.r + EPS)
        {
            ball nxta; nxta.pos = { pos.x + speed.x * PDCK, pos.y + speed.y * PDCK };
            ball nxtb; nxtb.pos = { b.pos.x + b.speed.x * PDCK, b.pos.y + b.speed.y * PDCK };
            if (nxta.dis(nxtb) <= r + b.r + EPS)   // 如果当前发生碰撞PDCK帧后还存在碰撞
                return true;
            else return false;
        }
        return false;
    }
    
    // 施加一个力
    void makespeed(double direction, double extent)  
    {
        speed.x += sin(direction / 180.000 * pi) * extent;
        speed.y += cos(direction / 180.000 * pi) * extent;
    }
    
    // 使用二维向量作为输入施加一个力
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
    
    // 根据速度来移动球
    void move()                                     
    {
        pos.x += speed.x;
        pos.y += speed.y;
        
        // 应用摩擦力
        speed.x *= FRICTION;
        speed.y *= FRICTION;
        
        // 如果速度很小，则停止
        if (fabs(speed.x) < MIN_SPEED && fabs(speed.y) < MIN_SPEED) {
            speed.x = 0;
            speed.y = 0;
        }
    }
};

// 游戏状态枚举
enum GameState {
    AIMING,      // 瞄准阶段
    PLAYING,     // 球在运动中
    GAME_OVER    // 游戏结束
};

// 全局变量
LPDIRECT3D9 g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;
D3DPRESENT_PARAMETERS g_d3dpp;
std::vector<ball> balls;
GameState gameState = AIMING;
vec2 aimStart = { 0, 0 };
vec2 aimEnd = { 0, 0 };
bool isAiming = false;
float windowScale = 1.0f;
HWND g_hWnd = NULL;
int windowWidth = 800;
int windowHeight = 600;
bool gameRunning = true;

// 初始化DirectX
bool InitD3D(HWND hWnd)
{
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == NULL)
        return false;

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

// 释放DirectX资源
void Cleanup()
{
    if (g_pd3dDevice != NULL)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }

    if (g_pD3D != NULL)
    {
        g_pD3D->Release();
        g_pD3D = NULL;
    }
}

// 渲染球体
void DrawBall(ball b)
{
    if (b.isPotted) return;

    D3DXVECTOR2 center(b.pos.x * windowScale, b.pos.y * windowScale);
    float radius = b.r * windowScale;

    // 创建一个简单的圆形
    const int segments = 32;
    LPD3DXLINE line = nullptr; // 初始化指针为nullptr
    if (FAILED(D3DXCreateLine(g_pd3dDevice, &line))) {
        return; // 创建失败直接返回
    }

    D3DXVECTOR2 points[segments + 1];
    for (int i = 0; i <= segments; i++)
    {
        float angle = (float)(i * 2 * pi / segments);
        points[i].x = center.x + radius * cos(angle);
        points[i].y = center.y + radius * sin(angle);
    }

    // 绘制圆形轮廓
    line->SetWidth(2.0f);
    line->Draw(points, segments + 1, D3DCOLOR_XRGB(0, 0, 0));

    // 如果是白球且在瞄准状态，绘制瞄准线
    if (b.isPotted == false && gameState == AIMING && isAiming)
    {
        D3DXVECTOR2 aimPoints[2];
        aimPoints[0].x = center.x;
        aimPoints[0].y = center.y;
        aimPoints[1].x = aimEnd.x;
        aimPoints[1].y = aimEnd.y;

        line->SetWidth(2.0f);
        line->Draw(aimPoints, 2, D3DCOLOR_XRGB(255, 255, 255));
    }

    line->Release(); // 确保无论是否绘制瞄准线，都释放line

    // 填充圆形
    ID3DXSprite* sprite;
    D3DXCreateSprite(g_pd3dDevice, &sprite);

    // 创建一个圆形纹理
    LPDIRECT3DSURFACE9 surface;
    g_pd3dDevice->CreateOffscreenPlainSurface(
        (int)(radius * 2), (int)(radius * 2),
        D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surface, NULL);

    D3DLOCKED_RECT lockedRect;
    surface->LockRect(&lockedRect, NULL, 0);

    // 填充圆形区域
    unsigned int* pixels = (unsigned int*)lockedRect.pBits;
    for (int y = 0; y < (int)(radius * 2); y++)
    {
        for (int x = 0; x < (int)(radius * 2); x++)
        {
            float dx = x - radius;
            float dy = y - radius;
            if (dx * dx + dy * dy <= radius * radius)
            {
                pixels[y * (lockedRect.Pitch / 4) + x] = b.color;
            }
        }
    }

    // 如果isfancy为1，绘制内部白色圆
    if (b.isfancy)
    {
        float innerRadius = radius * 0.4f; // 内部白色圆半径为球半径的0.4倍
        for (int y = 0; y < (int)(radius * 2); y++)
        {
            for (int x = 0; x < (int)(radius * 2); x++)
            {
                float dx = x - radius;
                float dy = y - radius;
                if (dx * dx + dy * dy <= innerRadius * innerRadius)
                {
                    pixels[y * (lockedRect.Pitch / 4) + x] = WHITE_COLOR;
                }
            }
        }
    }

    surface->UnlockRect();

    // 创建纹理
    LPDIRECT3DTEXTURE9 texture;
    g_pd3dDevice->CreateTexture(
        (int)(radius * 2), (int)(radius * 2),
        1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &texture, NULL);

    // 将表面复制到纹理
    LPDIRECT3DSURFACE9 textureSurface;
    texture->GetSurfaceLevel(0, &textureSurface);
    D3DXLoadSurfaceFromSurface(textureSurface, NULL, NULL, surface, NULL, NULL, D3DX_FILTER_NONE, 0);
    textureSurface->Release();
    surface->Release();

    // 绘制纹理
    RECT srcRect = { 0, 0, (int)(radius * 2), (int)(radius * 2) };
    D3DXVECTOR3 pos(center.x - radius, center.y - radius, 0);
    sprite->Begin(D3DXSPRITE_ALPHABLEND);
    sprite->Draw(texture, &srcRect, NULL, &pos, D3DCOLOR_XRGB(255, 255, 255));
    sprite->End();

    // 释放资源
    texture->Release();
    sprite->Release();
}
// 渲染球桌
void DrawTable()
{
    // 绘制桌面
    D3DRECT tableRect = {
        0,
        0,
        (LONG)(TABLE_WIDTH * windowScale),
        (LONG)(TABLE_HEIGHT * windowScale)
    };
    g_pd3dDevice->Clear(1, &tableRect, D3DCLEAR_TARGET, TABLE_COLOR, 1.0f, 0);

    // 绘制边框
    RECT borderRect = {
        -10,
        -10,
        (LONG)(TABLE_WIDTH * windowScale + 10),
        (LONG)(TABLE_HEIGHT * windowScale + 10)
    };
    LPD3DXLINE line = nullptr; // 初始化指针为nullptr
    if (FAILED(D3DXCreateLine(g_pd3dDevice, &line))) {
        return; // 创建失败直接返回
    }
    line->SetWidth(20.0f);

    D3DXVECTOR2 borderPoints[5] = {
        { -10, -10 },
        { (float)(TABLE_WIDTH * windowScale + 10), -10 },
        { (float)(TABLE_WIDTH * windowScale + 10), (float)(TABLE_HEIGHT * windowScale + 10) },
        { -10, (float)(TABLE_HEIGHT * windowScale + 10) },
        { -10, -10 }
    };

    line->Draw(borderPoints, 5, BORDER_COLOR);

    // 绘制球洞
    D3DXVECTOR2 holes[6] = {
        { 0, 0 },
        { (float)(TABLE_WIDTH * windowScale / 2), 0 },
        { (float)(TABLE_WIDTH * windowScale), 0 },
        { 0, (float)(TABLE_HEIGHT * windowScale) },
        { (float)(TABLE_WIDTH * windowScale / 2), (float)(TABLE_HEIGHT * windowScale) },
        { (float)(TABLE_WIDTH * windowScale), (float)(TABLE_HEIGHT * windowScale) }
    };

    for (int i = 0; i < 6; i++)
    {
        D3DXVECTOR2 holePoints[32 + 1];
        for (int j = 0; j <= 32; j++)
        {
            float angle = (float)(j * 2 * pi / 32);
            holePoints[j].x = holes[i].x + HOLE_RADIUS * cos(angle);
            holePoints[j].y = holes[i].y + HOLE_RADIUS * sin(angle);
        }

        line->SetWidth(2.0f);
        line->Draw(holePoints, 32 + 1, BLACK_COLOR);

        // 填充黑洞
        ID3DXSprite* sprite;
        D3DXCreateSprite(g_pd3dDevice, &sprite);

        LPDIRECT3DSURFACE9 surface;
        g_pd3dDevice->CreateOffscreenPlainSurface(
            HOLE_RADIUS * 2, HOLE_RADIUS * 2,
            D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surface, NULL);

        D3DLOCKED_RECT lockedRect;
        surface->LockRect(&lockedRect, NULL, 0);

        unsigned int* pixels = (unsigned int*)lockedRect.pBits;
        for (int y = 0; y < HOLE_RADIUS * 2; y++)
        {
            for (int x = 0; x < HOLE_RADIUS * 2; x++)
            {
                float dx = x - HOLE_RADIUS;
                float dy = y - HOLE_RADIUS;
                if (dx * dx + dy * dy <= HOLE_RADIUS * HOLE_RADIUS)
                {
                    pixels[y * (lockedRect.Pitch / 4) + x] = BLACK_COLOR;
                }
            }
        }

        surface->UnlockRect();

        LPDIRECT3DTEXTURE9 texture;
        g_pd3dDevice->CreateTexture(
            HOLE_RADIUS * 2, HOLE_RADIUS * 2,
            1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8,
            D3DPOOL_DEFAULT, &texture, NULL);

        LPDIRECT3DSURFACE9 textureSurface;
        texture->GetSurfaceLevel(0, &textureSurface);
        D3DXLoadSurfaceFromSurface(textureSurface, NULL, NULL, surface, NULL, NULL, D3DX_FILTER_NONE, 0);
        textureSurface->Release();
        surface->Release();

        RECT srcRect = { 0, 0, HOLE_RADIUS * 2, HOLE_RADIUS * 2 };
        D3DXVECTOR3 pos(holes[i].x - HOLE_RADIUS, holes[i].y - HOLE_RADIUS, 0);
        sprite->Begin(D3DXSPRITE_ALPHABLEND);
        sprite->Draw(texture, &srcRect, NULL, &pos, D3DCOLOR_XRGB(255, 255, 255));
        sprite->End();

        texture->Release();
        sprite->Release();
    }

    line->Release(); // 释放line
}

// 渲染游戏状态
void DrawGameState()
{
    if (gameState == AIMING)
    {
        // 绘制提示文本
        std::wstring text = L"点击并拖动白球来瞄准，释放鼠标击球";
        RECT textRect = { 10, 10, 400, 50 };
        D3DCOLOR textColor = D3DCOLOR_XRGB(255, 255, 255);
        
        // 使用DirectX字体绘制文本
        ID3DXFont* font;
        D3DXCreateFontW(g_pd3dDevice, 20, 0, FW_BOLD, 1, FALSE, 
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, 
                      DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                      L"Arial", &font);
        
        font->DrawTextW(NULL, text.c_str(), -1, &textRect, DT_LEFT, textColor);
        font->Release();
    }
}

// 渲染场景
void Render()
{
    if (g_pd3dDevice == NULL)
        return;

    // 清除背景
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    // 开始渲染
    if (SUCCEEDED(g_pd3dDevice->BeginScene()))
    {
        // 绘制球桌
        DrawTable();

        // 绘制所有球
        for (auto& ball : balls)
        {
            DrawBall(ball);
        }

        // 绘制游戏状态
        DrawGameState();

        // 结束渲染
        g_pd3dDevice->EndScene();
    }

    // 交换缓冲区
    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
}

// 初始化球
void InitBalls()
{
    balls.clear();
    
    // 添加白球
    ball whiteBall;
    whiteBall.pos = { TABLE_WIDTH * 0.75, TABLE_HEIGHT * 0.5 };
    whiteBall.speed = { 0, 0 };
    whiteBall.r = BALL_RADIUS;
    whiteBall.color = WHITE_COLOR;
    whiteBall.isfancy = false;
    whiteBall.isPotted = false;
    balls.push_back(whiteBall);
    
    // 添加红球和花色球
    D3DCOLOR colors[] = {
        RED_COLOR, YELLOW_COLOR, GREEN_COLOR, BLUE_COLOR,
        PURPLE_COLOR, ORANGE_COLOR, BROWN_COLOR, BLACK_COLOR
    };
    
    // 创建三角形排列的球
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
            
            // 交替放置纯色球和花色球
            if (count % 2 == 0) {
                b.color = colors[count / 2 % 7];
                b.isfancy = false;
            } else {
                b.color = colors[count / 2 % 7];
                b.isfancy = true;
            }
            
            b.isPotted = false;
            balls.push_back(b);
            count++;
        }
    }
}

// 检查球是否入洞
void CheckPottedBalls()
{
    D3DXVECTOR2 holes[6] = {
        { 0, 0 },
        { (float)(TABLE_WIDTH * windowScale / 2), 0 },
        { (float)(TABLE_WIDTH * windowScale), 0 },
        { 0, (float)(TABLE_HEIGHT * windowScale) },
        { (float)(TABLE_WIDTH * windowScale / 2), (float)(TABLE_HEIGHT * windowScale) },
        { (float)(TABLE_WIDTH * windowScale), (float)(TABLE_HEIGHT * windowScale) }
    };
    
    for (auto& ball : balls)
    {
        if (ball.isPotted) continue;
        
        vec2 ballScreenPos = { 
            ball.pos.x * windowScale, 
            ball.pos.y * windowScale 
        };
        
        for (auto& hole : holes)
        {
            double dx = ballScreenPos.x - hole.x;
            double dy = ballScreenPos.y - hole.y;
            double distance = sqrt(dx * dx + dy * dy);
            
            if (distance < HOLE_RADIUS - 5) // 稍微小于洞的半径，确保球完全进入
            {
                ball.isPotted = true;
                
                // 如果白球入洞，将其重置到初始位置
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
        
        if (ball.speed.x != 0 || ball.speed.y != 0)
            return false;
    }
    return true;
}

// 物理引擎，刷新一帧
void UpdatePhysics()
{
    if (gameState != PLAYING) return;
    
    int numBalls = balls.size();
    
    // 处理球与球之间的碰撞
    for (int i = 0; i < numBalls; ++i)
    {
        if (balls[i].isPotted) continue;
        
        for (int j = i + 1; j < numBalls; ++j)
        {
            if (balls[j].isPotted) continue;
            
            if (balls[i].collision(balls[j]))
            {
                // 碰撞处理，根据动量守恒
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
                
                // 分离球体，避免粘连
                double overlap = balls[i].r + balls[j].r - dist;
                if (overlap > 0) {
                    balls[i].pos.x -= normal.x * overlap * 0.5;
                    balls[i].pos.y -= normal.y * overlap * 0.5;
                    balls[j].pos.x += normal.x * overlap * 0.5;
                    balls[j].pos.y += normal.y * overlap * 0.5;
                }
            }
        }
        
        // 处理球与边界的碰撞
        double leftBound = balls[i].r;
        double rightBound = TABLE_WIDTH - balls[i].r;
        double topBound = balls[i].r;
        double bottomBound = TABLE_HEIGHT - balls[i].r;
        
        if (balls[i].pos.x < leftBound)
        {
            balls[i].pos.x = leftBound;
            balls[i].speed.x = -balls[i].speed.x * 0.8; // 能量损失
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
    
    // 移动所有球
    for (auto& ball : balls)
    {
        if (!ball.isPotted)
            ball.move();
    }
    
    // 检查球是否入洞
    CheckPottedBalls();
    
    // 检查是否所有球都停止运动
    if (AreAllBallsStopped())
    {
        gameState = AIMING;
    }
}

// 窗口过程
LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
        
    case WM_SIZE:
        // 处理窗口大小变化
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            
            // 计算窗口缩放比例，保持球桌的宽高比
            float scaleX = (float)width / TABLE_WIDTH;
            float scaleY = (float)height / TABLE_HEIGHT;
            windowScale = std::min(scaleX, scaleY);
            
            // 更新窗口尺寸
            windowWidth = width;
            windowHeight = height;
            
            // 更新DirectX设备
            if (g_pd3dDevice != NULL)
            {
                g_d3dpp.BackBufferWidth = width;
                g_d3dpp.BackBufferHeight = height;
                
                g_pd3dDevice->Reset(&g_d3dpp);
            }
        }
        return 0;
        
    case WM_LBUTTONDOWN:
        // 处理鼠标左键按下
        {
            if (gameState == AIMING)
            {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                
                // 检查是否点击在白球上
                ball& whiteBall = balls[WHITE_BALL_INDEX];
                vec2 ballScreenPos = { 
                    whiteBall.pos.x * windowScale, 
                    whiteBall.pos.y * windowScale 
                };
                
                double dx = x - ballScreenPos.x;
                double dy = y - ballScreenPos.y;
                double distance = sqrt(dx * dx + dy * dy);
                
                if (distance <= whiteBall.r * windowScale)
                {
                    isAiming = true;
                    aimStart = { (double)x, (double)y };
                    aimEnd = { (double)x, (double)y };
                }
            }
        }
        return 0;
        
    case WM_MOUSEMOVE:
        // 处理鼠标移动
        {
            if (isAiming)
            {
                int x = LOWORD(lParam);
                int y = HIWORD(lParam);
                aimEnd = { (double)x, (double)y };
            }
        }
        return 0;
        
    case WM_LBUTTONUP:
        // 处理鼠标左键释放
        {
            if (isAiming && gameState == AIMING)
            {
                isAiming = false;
                
                // 计算击球力度和方向
                double dx = aimStart.x - aimEnd.x;
                double dy = aimStart.y - aimEnd.y;
                double distance = sqrt(dx * dx + dy * dy);
                
                // 限制最大力度
                double maxPower = 10.0;
                double power = std::min(distance / 50.0, maxPower);
                
                if (power > 0.1) // 最小力度阈值
                {
                    // 应用力到白球
                    ball& whiteBall = balls[WHITE_BALL_INDEX];
                    vec2 force = { dx, dy };
                    whiteBall.makespeedEx(force, power*POWER);
                    
                    // 切换到游戏进行状态
                    gameState = PLAYING;
                }
            }
        }
        return 0;
    }
    
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// 主函数
INT WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, INT)
{
    // 注册窗口类
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, MsgProc, 0L, 0L,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
        L"Billiards Game", NULL };
    RegisterClassExW(&wc);
    
    // 创建窗口
    g_hWnd = CreateWindowA("Billiards Game", "台球游戏",
        WS_OVERLAPPEDWINDOW, 100, 100, 800, 600,
        NULL, NULL, wc.hInstance, NULL);
    
    // 初始化DirectX
    if (InitD3D(g_hWnd))
    {
        // 初始化球
        InitBalls();
        
        // 显示窗口
        ShowWindow(g_hWnd, SW_SHOWDEFAULT);
        UpdateWindow(g_hWnd);
        
        // 消息循环
        MSG msg;
        ZeroMemory(&msg, sizeof(msg));
        
        while (msg.message != WM_QUIT && gameRunning)
        {
            if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                // 更新物理
                UpdatePhysics();
                
                // 渲染场景
                Render();
            }
        }
    }
    
    // 清理资源
    Cleanup();
    UnregisterClassW(L"Billiards Game", wc.hInstance);
    
    return 0;
}