#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <ImGuiFileDialog.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "json.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include <future>

// Migrate to Drizzle3D

using json = nlohmann::json;
namespace fs = std::filesystem;

enum NodeType {
    FILE_NODE,
    DIRECTORY_NODE
};

struct TreeNode {
    std::string path;
    std::string name;
    std::string PJName;
    NodeType type;
    std::vector<TreeNode*> children;

    TreeNode(const std::string& name, const std::string& basePath, NodeType type) : name(name), type(type) {
        path = std::filesystem::current_path().string() + basePath;
        if (basePath == "") {
            path = std::filesystem::current_path().string() + basePath;
        }
        int a = 0;
    }

    ~TreeNode() {
        for (auto& child : children) {
            delete child;
        }
    }
};

void ParseJSON(TreeNode& treeNode, const std::string& basePath, const std::string& ProjectName, bool s = true) {
    fs::path path(basePath);
    if (fs::exists(path) && fs::is_directory(path)) {
        treeNode.name = path.filename().string();
        treeNode.type = DIRECTORY_NODE;
        for (const auto& entry : fs::directory_iterator(path)) {
            TreeNode* childNode = new TreeNode(entry.path().filename().string(), basePath, entry.is_directory() ? DIRECTORY_NODE : FILE_NODE);
            treeNode.children.push_back(childNode);
            if (entry.is_directory()) {
                ParseJSON(*childNode, entry.path().string(), ProjectName, false);
            }
        }
    }
    else {
        std::cerr << "Invalid directory path: " << basePath << std::endl;
    }
    if (s)
        treeNode.PJName = ProjectName;
}


std::pair<std::string, std::pair<std::string, std::pair<std::string, bool>>> DisplayTreeNode(TreeNode& node) {
    std::string content;
    std::string filePath = "";
    std::string fileNamea;
    bool bol = false;
    if (node.type == DIRECTORY_NODE) {
        std::string s = node.name;
        if (node.PJName != "") {
            s = node.PJName;
        }
        if (ImGui::TreeNode(s.c_str())) {
            for (auto& child : node.children) {
                std::pair<std::string, std::pair<std::string, std::pair<std::string, bool>>> h = DisplayTreeNode(*child);
                content += h.first;
                bol = h.second.second.second;
                if (bol) {
                    filePath = h.second.first;
                    fileNamea = h.second.second.first;
                }
            }
            ImGui::TreePop();
        }
    }
    else {
        if (ImGui::Selectable(node.name.c_str())) {
            std::string fileName = node.name;
            fileNamea = node.name;
            if (node.path != "") {
                fileName = node.path + "\\" + node.name;
                filePath = node.path;
            }
            std::string fileContent;
            std::ifstream file(fileName);
            std::string line;
            while (std::getline(file, line)) {
                fileContent += line + "\n";
            }
            content = fileContent;
            bol = true;
        }
    }
    return { content, {filePath, {fileNamea, bol} } };
}

void executeCommand(const std::string& command, std::string& s, std::atomic<bool>& h) {
    std::ostringstream output;
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "POPEN Failed!";
    }
    char buffer[128];
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != NULL)
            output << buffer;
    }
    _pclose(pipe);
    s = output.str();
    h.store(true);
}

int main(int argc, char* argv[]) {
    int widtha, heighta;
    std::thread t;
    std::atomic<bool> done(false);
    std::ifstream file("project.json");
    json fileTreeJson;
    file >> fileTreeJson;
    std::stack<std::string> undoStack;
    std::stack<std::string> redoStack;
    bool settingsOpen = false;
    ImFont* font;
    ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImVec4 bgColor = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
    ImGuiFileDialog fileDialog;
    std::string filePathName, filePath, text;
    std::string output;

    glfwInit();
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Drizzle Studio Lite", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize glad" << std::endl;
        return -1;
    }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    glfwGetWindowSize(window, &widtha, &heighta);
    //ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        glClearColor(bgColor.x, bgColor.y, bgColor.z, bgColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open", "Ctrl+O")) {
                    IGFD::FileDialogConfig config; config.path = ".";
                    fileDialog.OpenDialog("ChooseFileDlg", "Choose File", ".cpp,.h,.hpp", config);
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    if (!filePathName.empty()) {
                        std::ofstream ofs(filePath + "/" + filePathName, std::ofstream::trunc); // Construct the full file path
                        ofs << text;
                        ofs.close();
                    }
                    else {
                        std::ofstream ofs(filePathName, std::ofstream::trunc); // Construct the full file path
                        ofs << text;
                        ofs.close();
                    }
                }
                if (ImGui::MenuItem("Compile", "F5")) {
                    std::string a = "cmake . -B build && cmake --build build --config Release";
                    if (fileTreeJson.contains("script")) {
                        a = ((std::string)fileTreeJson["script"]).c_str();
                    }
                    t = std::thread(executeCommand, a, std::ref(output), std::ref(done));
                    
                    //output = future.get();
                    //exit(0);
                }

                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", nullptr, !undoStack.empty())) {
                    redoStack.push(text);
                    text = undoStack.top();
                    undoStack.pop();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", nullptr, !redoStack.empty())) {
                    undoStack.push(text);
                    text = redoStack.top();
                    redoStack.pop();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Settings")) {
                if (ImGui::MenuItem("Open Settings")) {
                    settingsOpen = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_Once);
        if (fileDialog.Display("ChooseFileDlg")) {
            if (fileDialog.IsOk()) {
                filePathName = fileDialog.GetFilePathName();
                filePath = fileDialog.GetCurrentPath();

                std::string myText;
                std::ifstream MyReadFile(filePath + "\\" + filePathName);

                while (getline(MyReadFile, myText)) {
                    text += myText + "\n";
                }

                MyReadFile.close();
                undoStack.push(text);
            }

            fileDialog.Close();
        }

        TreeNode root("", "", DIRECTORY_NODE);
        std::string basePath = fileTreeJson["root"];
        ParseJSON(root, basePath, fileTreeJson["project"]);

        int width, height;
        glfwGetWindowSize(window, &width, &height);
        float explorerWidth = static_cast<float>(width) * 0.25f;

        if (widtha != width || heighta != height) {
            ImGui::SetNextWindowSize(ImVec2(explorerWidth, height - 20), ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(width - explorerWidth, 20), ImGuiCond_Always);
        }
        else {
            ImGui::SetNextWindowSize(ImVec2(explorerWidth, height - 20), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(width - explorerWidth, 20), ImGuiCond_Once);
        }
        ImGui::Begin("Project Explorer");
        std::pair<std::string, std::pair<std::string, std::pair<std::string, bool>>> hs = DisplayTreeNode(root);
        if (hs.first != "") {
            text = hs.first;
            filePath = hs.second.first;
            filePathName = hs.second.second.first;
        }
        ImGui::End();

        float TextEditWidth = static_cast<float>(width) * 0.55f;
        float TextEditHeight = static_cast<float>(height) * 0.75f;
        if (widtha != width || heighta != height) {
            ImGui::SetNextWindowSize(ImVec2(TextEditWidth, TextEditHeight - 20), ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(static_cast<float>(width) * 0.20f, 20), ImGuiCond_Always);
        }
        else {
            ImGui::SetNextWindowSize(ImVec2(TextEditWidth, TextEditHeight - 20), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(static_cast<float>(width) * 0.20f, 20), ImGuiCond_Once);
        }
        ImGui::Begin("Text Editor");
        if (ImGui::InputTextMultiline("##textinput", &text, ImVec2(ImGui::GetWindowSize()[0], ImGui::GetWindowSize()[1] - 35))) {
            undoStack.push(text);
        }
        ImGui::End();

        float OutputWidth = static_cast<float>(width) * 0.55f;
        float OutputHeight = static_cast<float>(height) * 0.25f;
        if (widtha != width || heighta != height) {
            ImGui::SetNextWindowSize(ImVec2(OutputWidth, OutputHeight), ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(static_cast<float>(width) * 0.20f, height - OutputHeight), ImGuiCond_Always);
        }
        else {
            ImGui::SetNextWindowSize(ImVec2(OutputWidth, OutputHeight), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(static_cast<float>(width) * 0.20f, height - OutputHeight), ImGuiCond_Once);
        }
        ImGui::Begin("Output");
        ImGui::TextUnformatted(output.c_str());
        ImGui::End();

        if (settingsOpen) {
            // TODO: Center in Middle and Size to Good size.
            ImGui::Begin("Settings", &settingsOpen);
            ImGui::ColorEdit3("Background Color", (float*)&bgColor);
            ImGui::End();
        }

        if (t.joinable() && done.load())
            t.join();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        widtha = width;
        heighta = height;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
