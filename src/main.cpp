#define GLFW_INCLUDE_VULKAN
#include "./glfw-3.5.1/include/GLFW/glfw3.h"
#include "./1.4.357.0./include/vulkan/vulkan.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>

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
    uint32_t queueFamilyIndex = 0;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    // ... (이미지 뷰, 렌더패스, 파이프라인, 프레임버퍼 등 멤버 변수 선언)
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;

    // Vulkan 구조체 초기화
    VkInstanceCreateInfo createInfo{};
    VkDeviceQueueCreateInfo queueCreateInfo{};
    VkPhysicalDeviceFeatures deviceFeatures{}; // 현재는 특별히 활성화할 기능이 없으므로 빈 상태로 둡니다.
    VkDeviceCreateInfo createDeviceInfo{};
    VkSwapchainCreateInfoKHR createSwapchainInfo{};

    // ----------------------------------------------------

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
    }

    void initVulkan() {
        // 객체 간의 의존성 때문에 이 순서가 바뀌면 안 됩니다!
        createInstance();            // Vulkan 인스턴스 생성
        setupDebugMessenger();       // 디버그 메신저 생성
        createSurface();             // 물리적 기기 평가 전에 서피스가 있어야 함
        pickPhysicalDevice();        // 서피스 지원 여부 확인
        createLogicalDevice();       // 선택된 기기를 바탕으로 큐 생성
        createSwapChain();           // 큐와 서피스 기반으로 스왑체인 생성
        createImageViews();          // 스왑체인 이미지에 대한 뷰 생성
        createRenderPass();          // 렌더 패스 생성
        createGraphicsPipeline();    // 렌더 패스가 있어야 파이프라인 생성 가능
        createFramebuffers();        // 파이프라인(렌더패스)과 이미지 뷰가 있어야 함
        createCommandPool();         // 커맨드 풀 생성
        createCommandBuffer();       // 풀이 있어야 버퍼 할당 가능
        createSyncObjects();         // 동기화 객체 생성
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            drawFrame(); // 작성한 렌더링 함수
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

    // Vulkan 인스턴스 생성 및 검증 계층 활성화
    void createInstance() {
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

    // 디버그 메신저 생성 및 연결
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

    // 서피스 생성
    void createSurface() {
        // 파라미터: (Vulkan 인스턴스, GLFW 창 객체 포인터, 커스텀 할당자, 생성된 Surface 핸들 저장용 변수)
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Window Surface 생성에 실패했습니다!");
        }
    }

    // 물리적 기기 선택
    void pickPhysicalDevice() {
        // 사용 가능한 GPU 목록을 가져와서 적합한 GPU를 선택하는 로직 구현
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("Vulkan을 지원하는 GPU를 찾을 수 없습니다!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // 평가 및 선택 코드
        // 각 GPU를 순회하며 평가
        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break; // 적합한 첫 번째 기기를 선택
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("적합한 GPU를 찾지 못했습니다!");
        }
    }

    // 기기 평가 함수 구현부
    bool isDeviceSuitable(VkPhysicalDevice device) {
        // 1. 기기의 기본 속성 및 지원하는 기능 조회
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        
        // 외장 그래픽 카드인지 확인 (옵션이지만 권장됨)
        bool isDiscrete = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
        
        // 2-1. 큐 패밀리 지원 여부 확인
        bool hasGraphicsQueue = false;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        
        uint32_t i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                hasGraphicsQueue = true;
                queueFamilyIndex = i;
                break;
            }
            i++;
        }

        // 2-2. 큐 패밀리가 화면 출력(Present)을 지원하는지 검사
        VkBool32 presentSupport = false;
        if (hasGraphicsQueue) {
            vkGetPhysicalDeviceSurfaceSupportKHR(device, queueFamilyIndex, surface, &presentSupport);
        }
        
        // 외장 그래픽이며 그래픽스 큐를 지원하면서 화면 출력을 지원하면 적합하다고 판정
        return isDiscrete && hasGraphicsQueue && presentSupport;
    }

    void createLogicalDevice() {
        // 생성할 큐 지정하기
        uint32_t graphicsFamilyIndex = queueFamilyIndex; // 앞서 찾은 그래픽스 큐 패밀리 인덱스

        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsFamilyIndex;

        // 만들고자 하는 큐의 개수
        queueCreateInfo.queueCount = 1;

        // 큐의 우선순위 지정 (필수, 0.0f ~ 1.0f 사이의 실수 값)
        float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        // 논리적 기기 설정
        createDeviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        // 1번에서 만든 큐 생성 정보 연결
        createDeviceInfo.pQueueCreateInfos = &queueCreateInfo;
        createDeviceInfo.queueCreateInfoCount = 1;

        // 2번에서 만든 기기 기능 연결
        createDeviceInfo.pEnabledFeatures = &deviceFeatures;

        // 활성화할 기기 확장(Device Extensions) 지정
        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        createDeviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createDeviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // (참고) 최신 Vulkan에서는 기기 레벨의 Validation Layers가 폐지되어 
        // 인스턴스 레벨의 설정을 따르지만, 구형 드라이버 호환성을 위해 
        // 예전처럼 layer 이름을 명시해 주는 것이 좋습니다.
        if (enableValidationLayers) {
            createDeviceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createDeviceInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createDeviceInfo.enabledLayerCount = 0;
        }

        // 최종적으로 논리적 기기 생성
        if (vkCreateDevice(physicalDevice, &createDeviceInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("논리적 기기 생성에 실패했습니다!");
        }

        // 생성된 논리적 기기에서 그래픽스 큐 핸들 가져오기
        // 파라미터: (논리적 기기, 큐 패밀리 인덱스, 큐 인덱스, 반환받을 큐 핸들 변수)
        // 큐 인덱스는 0부터 시작하며, 우리는 1개만 만들었으므로 0을 넘깁니다.
        // 큐(graphicsQueue)는 논리적 기기가 파괴될 때 함께 소멸하므로 따로 해제할 필요가 없음
        vkGetDeviceQueue(device, graphicsFamilyIndex, 0, &graphicsQueue);
    }

    // 스왑 체인 생성
    void createSwapChain() {
        // 1. 스왑 체인 지원 정보 확인 (Capabilities, Formats, Present Modes)
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        if (formatCount != 0) {
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        if (presentModeCount != 0) {
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
        }

        // 2. 표면 형식(Surface Format) 선택
        // 기본값으로는 첫 번째 형식을 선택하고, 최적의 형식(SRGB)이 있는지 찾습니다.
        VkSurfaceFormatKHR surfaceFormat = formats[0];
        for (const auto& availableFormat : formats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = availableFormat;
                break;
            }
        }

        // 3. 프레젠테이션 모드(Presentation Mode) 선택
        // VK_PRESENT_MODE_FIFO_KHR는 수직 동기화(V-Sync)와 유사하며 항상 지원이 보장됩니다.
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& availablePresentMode : presentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) { // 삼중 버퍼링(Mailbox) 선호
                presentMode = availablePresentMode;
                break;
            }
        }

        // 4. 해상도(Extent) 선택
        VkExtent2D extent;
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            extent = capabilities.currentExtent; // 창 관리자가 지정한 크기 사용
        } else {
            // GLFW에서 픽셀 단위의 창 크기를 가져옵니다. (레티나 디스플레이 등 대응)
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            extent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            // 지원하는 최소/최대 해상도 범위 내로 제한
            extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }

        // 5. 스왑 체인 이미지 개수 결정 (최소 개수보다 1개 더 많게 설정하여 대기 시간 감소)
        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount; // 최대 개수를 초과하지 않도록 제한
        }

        // 6. 스왑 체인 생성 정보 구조체 채우기
        createSwapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createSwapchainInfo.surface = surface;
        createSwapchainInfo.minImageCount = imageCount;
        createSwapchainInfo.imageFormat = surfaceFormat.format;
        createSwapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
        createSwapchainInfo.imageExtent = extent;
        createSwapchainInfo.imageArrayLayers = 1; // VR이 아닌 이상 항상 1
        createSwapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 직접 렌더링할 대상

        // 그래픽스 큐와 프레젠테이션 큐가 같다고 가정하여 독점 모드(EXCLUSIVE) 사용
        // (만약 두 큐가 다르다면 CONCURRENT 모드를 사용해야 합니다)
        createSwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        // 현재 표면의 변환 상태 유지 (예: 90도 회전 안 함)
        createSwapchainInfo.preTransform = capabilities.currentTransform;
        
        // 다른 창과의 알파 블렌딩 적용 안 함
        createSwapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        
        createSwapchainInfo.presentMode = presentMode;
        createSwapchainInfo.clipped = VK_TRUE; // 다른 창에 가려진 픽셀 렌더링 무시
        createSwapchainInfo.oldSwapchain = VK_NULL_HANDLE;

        // 스왑 체인 생성
        if (vkCreateSwapchainKHR(device, &createSwapchainInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("스왑 체인 생성에 실패했습니다!");
        }

        // 향후 참조하기 위해 멤버 변수에 저장
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
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
