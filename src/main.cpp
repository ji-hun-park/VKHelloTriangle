#define GLFW_INCLUDE_VULKAN
#include "./glfw-3.5.1/include/GLFW/glfw3.h"
#include "./1.4.357.0./include/vulkan/vulkan.h"
#include <iostream>
#include <stdexcept>
#include <vector>

class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    // --- 윈도우 관련 변수 ---
    GLFWwindow* window;

    // --- Vulkan 객체 핸들 변수 ---
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    // ... (이미지 뷰, 렌더패스, 파이프라인, 프레임버퍼 등 멤버 변수 선언)
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    // ----------------------------------------------------

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        // 객체 간의 의존성 때문에 이 순서가 바뀌면 안 됩니다!
        createInstance();
        setupDebugMessenger();
        createSurface();             // 물리적 기기 평가 전에 서피스가 있어야 함
        pickPhysicalDevice();        // 서피스 지원 여부 확인
        createLogicalDevice();       // 선택된 기기를 바탕으로 큐 생성
        createSwapChain();           // 큐와 서피스 기반으로 스왑체인 생성
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();    // 렌더 패스가 있어야 파이프라인 생성 가능
        createFramebuffers();        // 파이프라인(렌더패스)과 이미지 뷰가 있어야 함
        createCommandPool();
        createCommandBuffer();       // 풀이 있어야 버퍼 할당 가능
        createSyncObjects();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame(); // 우리가 작성했던 그 렌더링 함수
        }
        
        // 창이 닫혀도 GPU가 작업 중일 수 있으므로 완전히 끝날 때까지 대기
        vkDeviceWaitIdle(device); 
    }

    void cleanup() {
        // 생성의 역순으로 파괴
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);
        
        vkDestroyCommandPool(device, commandPool, nullptr);
        // ... (프레임버퍼, 파이프라인, 렌더패스, 스왑체인 파괴)
        
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        // 디버그 메신저 파괴
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    // ... (각 create 함수들의 세부 구현부) ...
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
