// main.cpp
// This file copied from the GpuSchool sample. It initializes GLFW, GLAD, ImGui,
// sets up an OpenGL instanced renderer, and connects to the CUDA simulation via
// a C API. It has been included here as a part of the final-cleaned minimal
// project folder for build demonstration and distribution.

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdio.h>
#include <chrono>
#include <vector>
#include <algorithm>

// CUDA functions
extern "C" {
	void* createSimulation(int numAgents, float boundsSize);
	void destroySimulation(void* handle);
	void registerVBO(void* handle, unsigned int vbo);
	void updateAndWriteInstances(void* handle, float deltaTime);
	void setParameters(void* handle, float separation, float alignment, float cohesion);
}

// Instance structure - must match CUDA kernel
struct Instance {
	float x, y, z, scale;
	float qx, qy, qz, qw;
};

// Global state
int g_agentCount = 10000;
float g_boundsSize = 200.0f;
void* g_simulation = nullptr;

GLuint g_shaderProgram = 0;
GLuint g_vao = 0;
GLuint g_vbo = 0;
GLuint g_instanceVBO = 0;
// Debug point VAO + shader
GLuint g_pointVao = 0;
GLuint g_pointVbo = 0;
GLuint g_pointProgram = 0;
GLuint g_spriteTex = 0;

glm::vec3 g_cameraPos(0, -320, 120);
glm::vec3 g_cameraTarget(0, 0, 0);
float g_cameraYaw = 90.0f;
float g_cameraPitch = -12.0f;
float g_cameraDistance = 300.0f;

// Parameters
float g_separationWeight = 1.5f;
float g_alignmentWeight = 1.0f;
float g_cohesionWeight = 1.0f;

// Visual parameters
float g_fogDensity = 0.006f;
float g_causticsScale = 0.06f;
float g_causticsSpeed = 0.5f;
float g_causticsStrength = 0.55f;
float g_softParticleRange = 1.5f;
float g_glowStrength = 1.8f;
float g_waterScatter = 0.25f;

// FPS tracking
std::chrono::high_resolution_clock::time_point g_lastFrame;
float g_fps = 0.0f;
float g_time = 0.0f;

// Compile shader
GLuint compileShader(GLenum type, const char* source) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);
    
	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetShaderInfoLog(shader, 512, nullptr, infoLog);
		printf("Shader compilation failed:\n%s\n", infoLog);
	}
	return shader;
}

// Create shader program
void createShaders() {
	const char* vertexShaderSource = R"(
		#version 450 core
		layout(location = 0) in vec3 aVertexPos;
		layout(location = 1) in vec4 aInstancePos;
		layout(location = 2) in vec4 aInstanceQuat;
        
		uniform mat4 uViewProj;
		uniform mat4 uView;
		uniform vec3 uCameraPos;
        
		out vec3 vWorldPos;
		out vec3 vViewPos;
		out vec2 vUV;
		out vec3 vVelocity;
		out float vFishId;
        
		vec3 rotateByQuat(vec3 v, vec4 q) {
			vec3 qvec = q.xyz;
			vec3 uv = cross(qvec, v);
			vec3 uuv = cross(qvec, uv);
			return v + ((uv * q.w) + uuv) * 2.0;
		}
        
		// Extract forward direction from quaternion (velocity direction)
		vec3 quatToForward(vec4 q) {
			return vec3(
				2.0 * (q.x * q.z + q.w * q.y),
				2.0 * (q.y * q.z - q.w * q.x),
				1.0 - 2.0 * (q.x * q.x + q.y * q.y)
			);
		}
        
		void main() {
			// Treat aVertexPos.xy as billboard coordinates, preserve z for profile
			vec2 local = aVertexPos.xy * aInstancePos.w; // scale in XY plane
			vec3 rotatedLocal = rotateByQuat(vec3(local.x, local.y, 0.0), aInstanceQuat);
			vec3 rotated = rotatedLocal;
			vec3 worldPos = rotated + aInstancePos.xyz;
			vWorldPos = worldPos;
			vViewPos = (uView * vec4(worldPos, 1.0)).xyz;
			vVelocity = quatToForward(aInstanceQuat);
			gl_Position = uViewProj * vec4(worldPos, 1.0);
			vUV = aVertexPos.xy * 0.5 + 0.5;
			// Generate stable fish ID from position
			vFishId = fract(aInstancePos.x * 0.123 + aInstancePos.y * 0.456 + aInstancePos.z * 0.789);
		}
	)";
    
	const char* fragmentShaderSource = R"(
		#version 450 core
		in vec3 vWorldPos;
		in vec3 vViewPos;
		in vec2 vUV;
		in vec3 vVelocity;
		in float vFishId;
        
		uniform vec3 uCameraPos;
		uniform float uTime;
		uniform float uFogDensity;
		uniform float uCausticsScale;
		uniform float uCausticsSpeed;
		uniform float uCausticsStrength;
		uniform float uGlowStrength;
		uniform float uWaterScatter;
		uniform sampler2D uSprite;
        
		out vec4 fragColor;
        
		// Simple 3D noise for caustics
		float hash(vec3 p) {
			p = fract(p * 0.3183099 + 0.1);
			p *= 17.0;
			return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
		}
        
		float noise(vec3 p) {
			vec3 i = floor(p);
			vec3 f = fract(p);
			f = f * f * (3.0 - 2.0 * f);
            
			return mix(
				mix(mix(hash(i + vec3(0,0,0)), hash(i + vec3(1,0,0)), f.x),
					mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
				mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
					mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y),
				f.z);
		}
        
		// Enhanced multi-layer caustics
		float caustics(vec3 pos, float time) {
			vec2 uv = pos.xy * uCausticsScale;
			vec2 uv1 = uv + vec2(time * uCausticsSpeed * 0.5, time * uCausticsSpeed * 0.3);
			vec2 uv2 = uv - vec2(time * uCausticsSpeed * 0.3, time * uCausticsSpeed * 0.4);
			vec2 uv3 = uv * 1.5 + vec2(time * uCausticsSpeed * 0.2, -time * uCausticsSpeed * 0.25);
            
			float c1 = noise(vec3(uv1 * 2.0, time * 0.2));
			float c2 = noise(vec3(uv2 * 2.5, time * 0.15));
			float c3 = noise(vec3(uv3 * 1.8, time * 0.18));
            
			return min(min(c1, c2), c3) * 2.0;
		}
        
		// Volumetric god rays effect
		float godRays(vec3 pos, vec3 viewDir, float time) {
			vec3 rayPos = pos + viewDir * 10.0;
			float rays = noise(vec3(rayPos.xy * 0.02, time * 0.1));
			rays = pow(rays, 3.0);
			return rays * 0.4;
		}
        
		void main() {
			// Soft particle edge fade
			float dist = length(vUV - 0.5) * 2.0;
			float edgeFade = 1.0 - smoothstep(0.3, 1.0, dist);
            
			// Velocity-based color (speed modulates brightness)
			float speed = length(vVelocity);
			float speedFactor = clamp(speed / 25.0, 0.3, 1.0);
            
			// Multiple fish species with vibrant tropical colors
			vec3 fishColor;
			if (vFishId < 0.2) {
				// Electric blue tang
				fishColor = vec3(0.15, 0.6, 1.0) * 1.3;
			} else if (vFishId < 0.4) {
				// Bright orange clownfish
				fishColor = vec3(1.0, 0.5, 0.15) * 1.4;
			} else if (vFishId < 0.6) {
				// Yellow tang
				fishColor = vec3(1.0, 0.95, 0.2) * 1.2;
			} else if (vFishId < 0.8) {
				// Purple royal dottyback
				fishColor = vec3(0.7, 0.3, 1.0) * 1.3;
			} else {
				// Teal/cyan chromis
				fishColor = vec3(0.2, 0.9, 0.8) * 1.2;
			}
            
			// Add subtle iridescence based on viewing angle
			vec3 viewDir = normalize(uCameraPos - vWorldPos);
			vec3 normal = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
			float fresnel = pow(1.0 - abs(dot(viewDir, normal)), 2.0);
			vec3 iridescence = vec3(0.5, 0.8, 1.0) * fresnel * 0.4;
			vec3 baseColor = fishColor + iridescence;
            
			// Animate subtle body shimmer
			float shimmer = sin(vWorldPos.x * 0.2 + vWorldPos.y * 0.2 + uTime * 2.0) * 0.5 + 0.5;
			baseColor *= (1.0 + shimmer * 0.15);
            
			// Enhanced caustics lighting with multiple layers
			float causticPattern = caustics(vWorldPos, uTime);
			float causticFactor = 1.0 + uCausticsStrength * causticPattern;
			baseColor *= causticFactor;
            
			// God rays volumetric lighting
			float rays = godRays(vWorldPos, viewDir, uTime);
			baseColor += vec3(0.6, 0.8, 1.0) * rays * uWaterScatter;
            
			// Enhanced subsurface scattering with strong glow
			float rim = pow(max(1.0 - abs(dot(viewDir, normal)), 0.0), 2.0);
			vec3 glowColor = fishColor * 1.5 + vec3(0.3, 0.5, 0.8); // Fish body color glow
			baseColor += glowColor * rim * uGlowStrength;
            
			// Add bright edge highlight for pop
			float edgeGlow = pow(max(1.0 - abs(dot(viewDir, normal)), 0.0), 4.0);
			baseColor += vec3(1.0, 1.0, 1.0) * edgeGlow * 0.8;
            
			// Enhanced atmospheric fog with scattering
			float depth = length(vViewPos);
			float fogFactor = 1.0 - exp(-depth * uFogDensity);
			// Rich tropical water color with more blue-green
			vec3 fogColor = vec3(0.05, 0.2, 0.3) * (1.0 + causticPattern * 0.3);
			// Add light scattering in water
			fogColor += vec3(0.15, 0.25, 0.4) * uWaterScatter * (1.0 - fogFactor);
			vec3 finalColor = mix(baseColor, fogColor, fogFactor);
            
			// Brighter ambient for visibility
			finalColor += vec3(0.08, 0.12, 0.18) * (1.0 - fogFactor * 0.4);
            
			// Vibrant color grading with saturation boost
			finalColor = pow(finalColor, vec3(0.85)) * 1.35;
			// Increase saturation
			float luma = dot(finalColor, vec3(0.299, 0.587, 0.114));
			finalColor = mix(vec3(luma), finalColor, 1.3);
            
			// Texture sample-based alpha with improved soft masking
			vec4 tex = texture(uSprite, vUV);
			float maskAlpha = tex.a;
			// Enhanced soft circular mask fallback
			float r = length(vUV - 0.5);
			float circleAlpha = smoothstep(0.55, 0.4, r);
			float alpha = maskAlpha * circleAlpha;
			alpha *= (1.0 - fogFactor * 0.6) * speedFactor * 1.2;
            
			// Brighten fish with sprite gradient
			vec3 outColor = finalColor * mix(vec3(1.0), tex.rgb * 1.3, tex.a);
			fragColor = vec4(outColor, clamp(alpha, 0.0, 1.0));
		}
	)";
    
	GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    
	g_shaderProgram = glCreateProgram();
	glAttachShader(g_shaderProgram, vertexShader);
	glAttachShader(g_shaderProgram, fragmentShader);
	glLinkProgram(g_shaderProgram);
    
	GLint success;
	glGetProgramiv(g_shaderProgram, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetProgramInfoLog(g_shaderProgram, 512, nullptr, infoLog);
		printf("Shader linking failed:\n%s\n", infoLog);
	}
    
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

// Create a simple point shader for debugging instance positions
void createPointShader() {
	const char* vert = R"(
		#version 450 core
		layout(location = 0) in vec3 aVertexPos;
		layout(location = 1) in vec4 aInstancePos;
		uniform mat4 uViewProj;
		uniform float uPointScale;
		void main() {
			gl_Position = uViewProj * vec4(aInstancePos.xyz, 1.0);
			gl_PointSize = aInstancePos.w * uPointScale;
		}
	)";

	const char* frag = R"(
		#version 450 core
		out vec4 fragColor;
		void main() {
			fragColor = vec4(1.0, 0.6, 0.0, 1.0);
		}
	)";

	GLuint vs = compileShader(GL_VERTEX_SHADER, vert);
	GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag);
	g_pointProgram = glCreateProgram();
	glAttachShader(g_pointProgram, vs);
	glAttachShader(g_pointProgram, fs);
	glLinkProgram(g_pointProgram);
	GLint ok;
	glGetProgramiv(g_pointProgram, GL_LINK_STATUS, &ok);
	if (!ok) {
		char buf[512];
		glGetProgramInfoLog(g_pointProgram, 512, nullptr, buf);
		printf("Point shader link failed:\n%s\n", buf);
	}
	glDeleteShader(vs);
	glDeleteShader(fs);
}

// Create a procedural circular sprite texture (RGBA8) with gradient for fish-like appearance
void createSpriteTexture() {
	const int size = 128;
	std::vector<unsigned char> data(size * size * 4);
	for (int y = 0; y < size; ++y) {
		for (int x = 0; x < size; ++x) {
			float u = (x + 0.5f) / size;
			float v = (y + 0.5f) / size;
			float dx = (u - 0.5f) * 2.0f;
			float dy = (v - 0.5f) * 2.0f;
            
			// Create elongated fish shape with tail tapering
			float ex = dx / 2.0f;
			float ey = dy / 0.55f;
			// Add tail taper (narrower at back)
			float tailTaper = 1.0f - (std::max(0.0f, dx) * 0.4f);
			ey /= std::max(0.3f, tailTaper);
			float r = sqrtf(ex*ex + ey*ey);
            
			// Smooth falloff for soft edges with glow
			float t = (r - 0.8f) / (1.0f - 0.8f);
			t = std::clamp(t, 0.0f, 1.0f);
			float alpha = 1.0f - (t * t * (3.0f - 2.0f * t));
			// Add outer glow
			float glow = exp(-r * 3.0f) * 0.4f;
			alpha = std::clamp(alpha + glow, 0.0f, 1.0f);
            
			// Body-to-tail gradient (brighter center, darker edges)
			float bodyGradient = 1.0f - r * 0.2f;
			float tailGradient = (dx + 1.0f) * 0.3f + 0.5f; // Gradient along length
			float gradient = bodyGradient * tailGradient;
			gradient = std::clamp(gradient, 0.4f, 1.0f);
            
			unsigned char a = (unsigned char)(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
			unsigned char c = (unsigned char)(gradient * 255.0f);
            
			int idx = (y * size + x) * 4;
			data[idx + 0] = c;
			data[idx + 1] = c;
			data[idx + 2] = c;
			data[idx + 3] = a;
		}
	}

	glGenTextures(1, &g_spriteTex);
	glBindTexture(GL_TEXTURE_2D, g_spriteTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
}

// Create VAO used to draw instanced points (one vertex, instance attributes)
void createPointVAO() {
	glGenVertexArrays(1, &g_pointVao);
	glGenBuffers(1, &g_pointVbo);
	glBindVertexArray(g_pointVao);
	// Single placeholder vertex
	float v = 0.0f;
	glBindBuffer(GL_ARRAY_BUFFER, g_pointVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3, nullptr, GL_STATIC_DRAW);
	// Vertex attribute 0 dummy
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	// Instance attribute (position + scale)
	glBindBuffer(GL_ARRAY_BUFFER, g_instanceVBO);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)0);
	glVertexAttribDivisor(1, 1);

	glBindVertexArray(0);
}


// Create geometry
void createGeometry() {
	// Instanced quad for each fish (two triangles, 6 vertices)
	float vertices[] = {
		-1.0f, -1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
	};
    
	glGenVertexArrays(1, &g_vao);
	glGenBuffers(1, &g_vbo);
	glGenBuffers(1, &g_instanceVBO);
    
	glBindVertexArray(g_vao);
    
	// Vertex positions (per-vertex)
	glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    
	// Instance data (per-instance)
	glBindBuffer(GL_ARRAY_BUFFER, g_instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, g_agentCount * sizeof(Instance), nullptr, GL_DYNAMIC_DRAW);
    
	// Position + scale
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)0);
	glVertexAttribDivisor(1, 1);
    
	// Quaternion
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), (void*)(4 * sizeof(float)));
	glVertexAttribDivisor(2, 1);
    
	glBindVertexArray(0);
    
	printf("Geometry created: VAO=%u, VBO=%u, InstanceVBO=%u\n", g_vao, g_vbo, g_instanceVBO);
}

// Initialize simulation
void initSimulation() {
	g_simulation = createSimulation(g_agentCount, g_boundsSize);
	if (!g_simulation) {
		printf("Failed to create simulation!\n");
		return;
	}
    
	registerVBO(g_simulation, g_instanceVBO);
}

// Update camera based on mouse
void updateCamera() {
	float yaw = glm::radians(g_cameraYaw);
	float pitch = glm::radians(g_cameraPitch);
    
	g_cameraPos.x = g_cameraTarget.x + g_cameraDistance * cos(pitch) * cos(yaw);
	g_cameraPos.y = g_cameraTarget.y + g_cameraDistance * cos(pitch) * sin(yaw);
	g_cameraPos.z = g_cameraTarget.z + g_cameraDistance * sin(pitch);
}

// Debug: dump instance buffer once
bool g_dumpInstanceOnce = false;

// Mouse callback
double g_lastMouseX = 0.0;
double g_lastMouseY = 0.0;
bool g_mousePressed = false;

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		g_mousePressed = (action == GLFW_PRESS);
		if (g_mousePressed) {
			glfwGetCursorPos(window, &g_lastMouseX, &g_lastMouseY);
		}
	}
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
	if (g_mousePressed) {
		double dx = xpos - g_lastMouseX;
		double dy = ypos - g_lastMouseY;
        
		g_cameraYaw += dx * 0.2f;
		g_cameraPitch -= dy * 0.2f;
        
		g_cameraPitch = glm::clamp(g_cameraPitch, -89.0f, 89.0f);
        
		g_lastMouseX = xpos;
		g_lastMouseY = ypos;
	}
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	g_cameraDistance -= yoffset * 20.0f;
	g_cameraDistance = glm::clamp(g_cameraDistance, 50.0f, 1000.0f);
}

int main() {
	// Initialize GLFW
	if (!glfwInit()) {
		printf("Failed to initialize GLFW\n");
		return -1;
	}
    
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4);
    
	GLFWwindow* window = glfwCreateWindow(1920, 1080, "CUDA Fish Schooling - GPU Visualizer", nullptr, nullptr);
	if (!window) {
		printf("Failed to create window\n");
		glfwTerminate();
		return -1;
	}
    
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1); // VSync
    
	// Mouse callbacks
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetCursorPosCallback(window, cursorPosCallback);
	glfwSetScrollCallback(window, scrollCallback);
    
	// Initialize GLAD
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		printf("Failed to initialize GLAD\n");
		return -1;
	}
    
	printf("OpenGL %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
    
	// Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 450");
    
	// OpenGL state
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_MULTISAMPLE);
	// For point size shader
	glEnable(GL_PROGRAM_POINT_SIZE);
    
	// Create resources
	createShaders();
	createGeometry();
	createPointShader();
	createPointVAO();
	createSpriteTexture();
	initSimulation();
    
	g_lastFrame = std::chrono::high_resolution_clock::now();
    
	// Main loop
	while (!glfwWindowShouldClose(window)) {
		// Timing
		auto now = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float>(now - g_lastFrame).count();
		g_lastFrame = now;
		g_fps = 1.0f / deltaTime;
		g_time += deltaTime;
        
		glfwPollEvents();
        
		// Update camera
		updateCamera();
        
		// Update simulation
		updateAndWriteInstances(g_simulation, deltaTime);

		// Debug: dump first few instances from VBO to validate data written by CUDA
		if (g_dumpInstanceOnce) {
			const int dumpN = 5;
			std::vector<Instance> dumpBuf(dumpN);
			glBindBuffer(GL_ARRAY_BUFFER, g_instanceVBO);
			// Use glGetBufferSubData to read back first few instances
			glGetBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Instance) * dumpN, dumpBuf.data());
			for (int i = 0; i < dumpN; ++i) {
				printf("Instance[%d] pos=(%.2f, %.2f, %.2f) scale=%.3f quat=(%.3f, %.3f, %.3f, %.3f)\n",
					   i, dumpBuf[i].x, dumpBuf[i].y, dumpBuf[i].z, dumpBuf[i].scale,
					   dumpBuf[i].qx, dumpBuf[i].qy, dumpBuf[i].qz, dumpBuf[i].qw);
			}
			g_dumpInstanceOnce = false;
		}
        
		// Render with vibrant tropical aquarium background (deeper blue-teal)
		glClearColor(0.03f, 0.12f, 0.18f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
		// View-projection matrix
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
        
		glm::mat4 view = glm::lookAt(g_cameraPos, g_cameraTarget, glm::vec3(0, 0, 1));
		glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)width / height, 1.0f, 2000.0f);
		glm::mat4 viewProj = proj * view;
        
		glUseProgram(g_shaderProgram);
		glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "uViewProj"), 1, GL_FALSE, glm::value_ptr(viewProj));
		glUniformMatrix4fv(glGetUniformLocation(g_shaderProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
		glUniform3fv(glGetUniformLocation(g_shaderProgram, "uCameraPos"), 1, glm::value_ptr(g_cameraPos));
		glUniform1f(glGetUniformLocation(g_shaderProgram, "uTime"), g_time);
		glUniform1f(glGetUniformLocation(g_shaderProgram, "uFogDensity"), g_fogDensity);
		glUniform1f(glGetUniformLocation(g_shaderProgram, "uCausticsScale"), g_causticsScale);
		glUniform1f(glGetUniformLocation(g_shaderProgram, "uCausticsSpeed"), g_causticsSpeed);
		glUniform1f(glGetUniformLocation(g_shaderProgram, "uCausticsStrength"), g_causticsStrength);
		glUniform1f(glGetUniformLocation(g_shaderProgram, "uGlowStrength"), g_glowStrength);
		glUniform1f(glGetUniformLocation(g_shaderProgram, "uWaterScatter"), g_waterScatter);
        
		// Bind sprite texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, g_spriteTex);
		glUniform1i(glGetUniformLocation(g_shaderProgram, "uSprite"), 0);
        
		glBindVertexArray(g_vao);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, g_agentCount);
        
		// ImGui
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
        
		ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("FPS: %.1f", g_fps);
		ImGui::Text("Agents: %d", g_agentCount);
		ImGui::Separator();
        
		ImGui::Text("Boid Parameters");
		if (ImGui::SliderFloat("Separation", &g_separationWeight, 0.0f, 5.0f)) {
			setParameters(g_simulation, g_separationWeight, g_alignmentWeight, g_cohesionWeight);
		}
		if (ImGui::SliderFloat("Alignment", &g_alignmentWeight, 0.0f, 5.0f)) {
			setParameters(g_simulation, g_separationWeight, g_alignmentWeight, g_cohesionWeight);
		}
		if (ImGui::SliderFloat("Cohesion", &g_cohesionWeight, 0.0f, 5.0f)) {
			setParameters(g_simulation, g_separationWeight, g_alignmentWeight, g_cohesionWeight);
		}
        
		ImGui::Separator();
		ImGui::Text("Visual Effects");
		ImGui::SliderFloat("Fog Density", &g_fogDensity, 0.0f, 0.05f);
		ImGui::SliderFloat("Caustics Scale", &g_causticsScale, 0.01f, 0.2f);
		ImGui::SliderFloat("Caustics Speed", &g_causticsSpeed, 0.0f, 1.0f);
		ImGui::SliderFloat("Caustics Strength", &g_causticsStrength, 0.0f, 1.0f);
		ImGui::SliderFloat("Glow Strength", &g_glowStrength, 0.0f, 3.0f);
		ImGui::SliderFloat("Water Scatter", &g_waterScatter, 0.0f, 1.0f);
        
		ImGui::Separator();
		ImGui::Text("Camera: Right-drag to rotate");
		ImGui::Text("         Scroll to zoom");
		ImGui::Text("Pos: (%.1f, %.1f, %.1f)", g_cameraPos.x, g_cameraPos.y, g_cameraPos.z);
        
		ImGui::End();
        
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
		glfwSwapBuffers(window);
	}
    
	// Cleanup
	destroySimulation(g_simulation);
	glDeleteProgram(g_shaderProgram);
	glDeleteVertexArrays(1, &g_vao);
	glDeleteBuffers(1, &g_vbo);
	glDeleteBuffers(1, &g_instanceVBO);
    
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
    
	glfwDestroyWindow(window);
	glfwTerminate();
    
	return 0;
}

