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

void ParseJSON(const json& nodeData, TreeNode& treeNode, const std::string& basePath) {
    if (nodeData.contains("name") && nodeData.contains("type")) {
        treeNode.name = nodeData["name"];
        std::string type = nodeData["type"];
        std::cout << "Parsing node: " << treeNode.name << ", Type: " << type << std::endl;
        if (type == "directory") {
            treeNode.type = DIRECTORY_NODE;
            if (nodeData.contains("children")) {
                for (const auto& child : nodeData["children"]) {
                    std::string childName = child["name"];
                    std::string childPath = basePath + "/" + childName;
                    TreeNode* childNode = new TreeNode(childName, basePath, NodeType::DIRECTORY_NODE);
                    treeNode.children.push_back(childNode);
                    ParseJSON(child, *childNode, childPath);
                }
            }
        }
        else if (type == "file") {
            treeNode.type = FILE_NODE;
        }
    }
}

std::string DisplayTreeNode(TreeNode& node) {
    std::string content;
    if (node.type == DIRECTORY_NODE) {
        if (ImGui::TreeNode(node.name.c_str())) {
            for (auto& child : node.children) {
                content += DisplayTreeNode(*child);
            }
            ImGui::TreePop();
        }
    }
    else {
        if (ImGui::Selectable(node.name.c_str())) {
            std::string fileName = node.name;
            if (node.path != "")
                fileName = node.path + "\\" + node.name;
            std::string fileContent;
            std::ifstream file(fileName);
            std::string line;
            while (std::getline(file, line)) {
                fileContent += line + "\n";
            }
            content = fileContent;
        }
    }
    return content;
}

std::string executeCommand(const char* cmd) {
    std::ostringstream stream;
    FILE* pipe = _popen(cmd, "r");
    if (!pipe) {
        throw std::runtime_error("_popen() failed!");
    }
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        stream << buffer;
    }
    _pclose(pipe);
    return stream.str();
}

int main(int argc, char* argv[]) {
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
    //ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // TODO: FIX SAVING ISSUE
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
                }
                if (ImGui::MenuItem("Compile", "F5")) {
                    std::cout << "Executing Command" << ((std::string)fileTreeJson["script"]).c_str() << "\n";
                    output = executeCommand(((std::string)fileTreeJson["script"]).c_str());
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
        std::string basePath = fileTreeJson["root"]["path"];
        ParseJSON(fileTreeJson["root"], root, basePath);

        int width, height;
        glfwGetWindowSize(window, &width, &height);

        ImGui::SetNextWindowSize(ImVec2(300, 700), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(width - 300, 20), ImGuiCond_Once);
        ImGui::Begin("Project Explorer");
        std::string hs = DisplayTreeNode(root);
        if (hs != "") {
            text = hs;
            // TODO: CHANGE FILENAME AND PATH VARS
        }
        ImGui::End();

        ImGui::SetNextWindowSize(ImVec2(980, 500), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(0, 20), ImGuiCond_Once);

        ImGui::Begin("Text Editor");
        if (ImGui::InputTextMultiline("##textinput", &text, ImVec2(ImGui::GetWindowSize()[0], ImGui::GetWindowSize()[1] - 35))) {
            undoStack.push(text);
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0, height - 200), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(980, 200), ImGuiCond_Once);
        ImGui::Begin("Output");
        ImGui::TextUnformatted(output.c_str());
        ImGui::End();

        if (settingsOpen) {
            // TODO: Center in Middle and Size to Good size.
            ImGui::Begin("Settings", &settingsOpen);
            ImGui::ColorEdit3("Text Color", (float*)&textColor);
            ImGui::ColorEdit3("Background Color", (float*)&bgColor);
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
