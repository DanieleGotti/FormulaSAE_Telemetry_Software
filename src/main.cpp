#include <GLFW/glfw3.h>
#include <iostream>

int main() {
    if (!glfwInit()) {
        std::cerr << "Errore: impossibile inizializzare GLFW" << std::endl;
        return -1;
    }

    // Creiamo finestra senza OpenGL (Vulkan-only)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Telemetry Vulkan", nullptr, nullptr);

    if (!window) {
        std::cerr << "Errore: impossibile creare la finestra" << std::endl;
        glfwTerminate();
        return -1;
    }

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
