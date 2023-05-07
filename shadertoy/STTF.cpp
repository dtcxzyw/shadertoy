/*
    SPDX-License-Identifier: Apache-2.0
    Copyright 2023 Yingwei Zheng
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
        http://www.apache.org/licenses/LICENSE-2.0
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "shadertoy/STTF.hpp"
#include "shadertoy/Support.hpp"
#include <fstream>
#pragma warning(push, 0)
#include <cpp-base64/base64.h>
#include <hello_imgui/hello_imgui.h>
#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#pragma warning(pop)

SHADERTOY_NAMESPACE_BEGIN

void ShaderToyTransmissionFormat::load(const std::string& filePath) {
    std::ifstream file{ filePath };
    if(!file) {
        Log(HelloImGui::LogLevel::Error, "Cannot open file %s", filePath.c_str());
        throw Error{};
    }

    try {
        nlohmann::json json;
        file >> json;

        json.at("metadata").get_to(metadata);
        std::unordered_map<std::string, Node*> nodeMap;
        for(auto node : json.at("nodes")) {
            std::unique_ptr<Node> nodeVal;
            switch(magic_enum::enum_cast<NodeClass>(node.at("class").get<std::string>()).value_or(NodeClass::Unknown)) {
                case NodeClass::RenderOutput: {
                    nodeVal = std::make_unique<RenderOutput>();
                    break;
                }
                case NodeClass::GLSLShader: {
                    nodeVal =
                        std::make_unique<GLSLShader>(node.at("source").get<std::string>(),
                                                     magic_enum::enum_cast<NodeType>(node.at("type").get<std::string>()).value());
                    break;
                }
                case NodeClass::Texture: {
                    const auto width = node.at("width").get<uint32_t>();
                    const auto height = node.at("height").get<uint32_t>();
                    const auto base64Data = node.at("data").get<std::string>();
                    const auto decodedData = base64_decode(base64Data);
                    const auto begin = std::bit_cast<const uint32_t*>(decodedData.data());
                    const auto end = begin + static_cast<ptrdiff_t>(width) * height;
                    nodeVal = std::make_unique<Texture>(width, height, std::vector<uint32_t>{ begin, end });
                    break;
                }
                case NodeClass::LastFrame: {
                    nodeVal =
                        std::make_unique<LastFrame>(node.at("ref").get<std::string>(),
                                                    magic_enum::enum_cast<NodeType>(node.at("type").get<std::string>()).value());
                    break;
                }
                case NodeClass::Keyboard: {
                    nodeVal = std::make_unique<Keyboard>();
                    break;
                }
                default: {
                    Log(HelloImGui::LogLevel::Error, "Unknown node class %s", node.at("class").get<std::string>().c_str());
                    throw Error{};
                }
            }
            nodeVal->name = node.at("name").get<std::string>();
            nodeMap.emplace(nodeVal->name, nodeVal.get());
            nodes.push_back(std::move(nodeVal));
        }
        for(auto& node : nodes) {
            if(node->getNodeClass() == NodeClass::LastFrame) {
                auto& lastFrame = dynamic_cast<LastFrame&>(*node);
                lastFrame.refNode = nodeMap.at(lastFrame.refNodeName);
            }
        }
        for(auto link : json.at("links")) {
            const auto start = link.at("start").get<std::string>();
            const auto end = link.at("end").get<std::string>();
            const auto filter = magic_enum::enum_cast<Filter>(link.at("filter").get<std::string>()).value();
            const auto wrapMode = magic_enum::enum_cast<Wrap>(link.at("wrapMode").get<std::string>()).value();
            const auto slot = link.at("slot").get<uint32_t>();
            links.emplace_back(nodeMap.at(start), nodeMap.at(end), filter, wrapMode, slot);
        }
    } catch(const std::exception& ex) {
        Log(HelloImGui::LogLevel::Error, "Failed to parse STTF file: %s", ex.what());
        throw Error{};
    }
}
void ShaderToyTransmissionFormat::save(const std::string& filePath) const {
    std::ofstream file{ filePath };
    if(!file) {
        Log(HelloImGui::LogLevel::Error, "Cannot open file %s", filePath.c_str());
        throw Error{};
    }

    try {
        nlohmann::json json;
        nlohmann::to_json(json["metadata"], metadata);
        auto& jsonNodes = json["nodes"];
        for(auto& node : nodes) {
            nlohmann::json jsonNode;
            jsonNode["class"] = magic_enum::enum_name(node->getNodeClass());
            jsonNode["name"] = node->name;

            switch(node->getNodeClass()) {
                case NodeClass::RenderOutput:
                    [[fallthrough]];
                case NodeClass::Keyboard:
                    [[fallthrough]];
                case NodeClass::SoundOutput: {
                    break;
                }
                case NodeClass::GLSLShader: {
                    const auto& shader = dynamic_cast<GLSLShader&>(*node);
                    jsonNode["source"] = shader.source;
                    jsonNode["type"] = magic_enum::enum_name(shader.nodeType);
                    break;
                }
                case NodeClass::Texture: {
                    const auto& texture = dynamic_cast<Texture&>(*node);
                    const auto bytes = as_bytes(std::span{ texture.pixel.begin(), texture.pixel.end() });
                    jsonNode["data"] = base64_encode(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
                    jsonNode["width"] = texture.width;
                    jsonNode["height"] = texture.height;
                    break;
                }
                case NodeClass::LastFrame: {
                    auto& lastFrame = dynamic_cast<LastFrame&>(*node);
                    jsonNode["ref"] = lastFrame.refNodeName;
                    jsonNode["type"] = magic_enum::enum_name(lastFrame.nodeType);
                    break;
                }
                default: {
                    reportNotImplemented();
                }
            }
            jsonNodes.push_back(jsonNode);
        }

        auto& jsonLinks = json["links"];
        for(const auto& [start, end, filter, wrapMode, slot] : links) {
            nlohmann::json jsonLink;
            jsonLink["start"] = start->name;
            jsonLink["end"] = end->name;
            jsonLink["filter"] = magic_enum::enum_name(filter);
            jsonLink["wrapMode"] = magic_enum::enum_name(wrapMode);
            jsonLink["slot"] = slot;
            jsonLinks.push_back(jsonLink);
        }

        file << json;
        file.close();
    } catch(const std::exception& ex) {
        Log(HelloImGui::LogLevel::Error, "Failed to write STTF file: %s", ex.what());
        throw Error{};
    }
}

SHADERTOY_NAMESPACE_END
