#define GLFW_INCLUDE_VULKAN
#include "./glfw-3.5.1/include/GLFW/glfw3.h"
#include "./1.4.357.0./include/vulkan/vulkan.h"
#include <iostream>
#include <stdexcept>
#include <vector>

// 검증 계층 활성화 여부를 결정하는 플래그 정의
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

// 활성화할 검증 계층 이름
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// 디버그 콜백을 사용하기 위한 필수 확장 기능
const std::vector<const char*> extensions = {
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME // "VK_EXT_debug_utils"와 동일
};

// Vulkan 디버그 콜백 함수 정의
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    // 오류(Error)나 경고(Warning) 메시지만 콘솔에 출력하도록 필터링
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation Layer: " << pCallbackData->pMessage << std::endl;
    }

    // 콜백이 무조건 VK_FALSE를 반환해야 Vulkan 호출이 취소되지 않고 계속 진행됩니다.
    return VK_FALSE;
}

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

    // 각 create 함수들의 세부 구현부
    void createInstance() {
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            
            // 확장 기능 추가 (GLFW 요구 확장 기능 + 디버그 확장 기능)
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();
        } else {
            createInfo.enabledLayerCount = 0;
            // 릴리즈 빌드용 확장 기능만 추가
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void setupDebugMessenger() {
        VkDebugUtilsMessengerEXT debugMessenger;

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        // 어떤 종류의 메시지(경고, 에러 등)를 받을지 설정
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        // 작성한 콜백 함수 연결
        debugCreateInfo.pfnUserCallback = debugCallback; 

        // 확장 함수 포인터 로드 및 호출
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            if (func(instance, &debugCreateInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
                throw std::runtime_error("failed to set up debug messenger!");
            }
        } else {
            throw std::runtime_error("Extension not present");
        }
    }
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
