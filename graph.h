#ifndef SUBGRAPHMATCHING_GRAPH_H
#define SUBGRAPHMATCHING_GRAPH_H

#include <unordered_map>
#include <iostream>
#include "config.h"
#include "types.h"
#include <fstream>
#include <vector>
#include <algorithm>
#include <chrono>

/**
 * A graph is stored as the CSR format.
 */

class Graph {
private:
    bool enable_label_offset_;

    ui vertices_count_;
    ui edges_count_;
    ui labels_count_;
    ui max_degree_;
    ui max_label_frequency_;

    ui* offsets_;
    VertexID * neighbors_;
    LabelID* labels_;
    ui* reverse_index_offsets_;
    ui* reverse_index_;

    int* core_table_;
    ui core_length_;

    std::unordered_map<LabelID, ui> labels_frequency_;

#if OPTIMIZED_LABELED_GRAPH == 1
    ui* labels_offsets_;
    std::unordered_map<LabelID, ui>* nlf_;
#endif

private:
    void BuildReverseIndex();

#if OPTIMIZED_LABELED_GRAPH == 1
    void BuildNLF();
    void BuildLabelOffset();
#endif

public:
    Graph(const bool enable_label_offset) {
        enable_label_offset_ = enable_label_offset;

        vertices_count_ = 0;
        edges_count_ = 0;
        labels_count_ = 0;
        max_degree_ = 0;
        max_label_frequency_ = 0;
        core_length_ = 0;

        offsets_ = NULL;
        neighbors_ = NULL;
        labels_ = NULL;
        reverse_index_offsets_ = NULL;
        reverse_index_ = NULL;
        core_table_ = NULL;
        labels_frequency_.clear();

#if OPTIMIZED_LABELED_GRAPH == 1
        labels_offsets_ = NULL;
        nlf_ = NULL;
#endif
    }

    ~Graph() {
        delete[] offsets_;
        delete[] neighbors_;
        delete[] labels_;
        delete[] reverse_index_offsets_;
        delete[] reverse_index_;
        delete[] core_table_;
#if OPTIMIZED_LABELED_GRAPH == 1
        delete[] labels_offsets_;
        delete[] nlf_;
#endif
    }

public:
    void loadGraphFromFile(const std::string& file_path);
    void loadGraphFromFileCompressed(const std::string& degree_path, const std::string& edge_path,
                                     const std::string& label_path);
    void storeComparessedGraph(const std::string& degree_path, const std::string& edge_path,
                               const std::string& label_path);
    void printGraphMetaData();
public:
    const ui getLabelsCount() const {
        return labels_count_;
    }

    const ui getVerticesCount() const {
        return vertices_count_;
    }

    const ui getEdgesCount() const {
        return edges_count_;
    }

    const ui getGraphMaxDegree() const {
        return max_degree_;
    }

    const ui getGraphMaxLabelFrequency() const {
        return max_label_frequency_;
    }

    const ui getVertexDegree(const VertexID id) const {
        return offsets_[id + 1] - offsets_[id];
    }

    const ui getLabelsFrequency(const LabelID label) const {
        return labels_frequency_.find(label) == labels_frequency_.end() ? 0 : labels_frequency_.at(label);
    }

    const ui getCoreValue(const VertexID id) const {
        return core_table_[id];
    }

    const ui get2CoreSize() const {
        return core_length_;
    }
    const LabelID getVertexLabel(const VertexID id) const {
        return labels_[id];
    }

    const ui * getVertexNeighbors(const VertexID id, ui& count) const {
        count = offsets_[id + 1] - offsets_[id];
        return neighbors_ + offsets_[id];
    }


    const ui * getVerticesByLabel(const LabelID id, ui& count) const {
        count = reverse_index_offsets_[id + 1] - reverse_index_offsets_[id];
        return reverse_index_ + reverse_index_offsets_[id];
    }

#if OPTIMIZED_LABELED_GRAPH == 1
    const ui * getNeighborsByLabel(const VertexID id, const LabelID label, ui& count) const {
        ui offset = id * labels_count_ + label;
        count = labels_offsets_[offset + 1] - labels_offsets_[offset];
        return neighbors_ + labels_offsets_[offset];
    }

    const std::unordered_map<LabelID, ui>* getVertexNLF(const VertexID id) const {
        return nlf_ + id;
    }

    bool checkEdgeExistence(const VertexID u, const VertexID v, const LabelID u_label) const {
        ui count = 0;
        const VertexID* neighbors = getNeighborsByLabel(v, u_label, count);
        int begin = 0;
        int end = count - 1;
        while (begin <= end) {
            int mid = begin + ((end - begin) >> 1);
            if (neighbors[mid] == u) {
                return true;
            }
            else if (neighbors[mid] > u)
                end = mid - 1;
            else
                begin = mid + 1;
        }

        return false;
    }
#endif

    bool checkEdgeExistence(VertexID u, VertexID v) const {
        if (getVertexDegree(u) < getVertexDegree(v)) {
            std::swap(u, v);
        }
        ui count = 0;
        const VertexID* neighbors =  getVertexNeighbors(v, count);

        int begin = 0;
        int end = count - 1;
        while (begin <= end) {
            int mid = begin + ((end - begin) >> 1);
            if (neighbors[mid] == u) {
                return true;
            }
            else if (neighbors[mid] > u)
                end = mid - 1;
            else
                begin = mid + 1;
        }

        return false;
    }

    void buildCoreTable();
};

//根据结点标签寻找结点，建立索引
void Graph::BuildReverseIndex() {
    reverse_index_ = new ui[vertices_count_];
    reverse_index_offsets_= new ui[labels_count_ + 1];
    reverse_index_offsets_[0] = 0;

    //为每种标签计算offset值
    ui total = 0;
    for (ui i = 0; i < labels_count_; ++i) {
        reverse_index_offsets_[i + 1] = total;
        total += labels_frequency_[i];
    }

    //将每个结点按照标签种类，依次放入索引表中
    for (ui i = 0; i < vertices_count_; ++i) {
        LabelID label = labels_[i];
        reverse_index_[reverse_index_offsets_[label + 1]++] = i;
    }
}

#if OPTIMIZED_LABELED_GRAPH == 1
void Graph::BuildNLF() {
    nlf_ = new std::unordered_map<LabelID, ui>[vertices_count_];
    for (ui i = 0; i < vertices_count_; ++i) {
        ui count;
        const VertexID * neighbors = getVertexNeighbors(i, count);

        for (ui j = 0; j < count; ++j) {
            VertexID u = neighbors[j];
            LabelID label = getVertexLabel(u);
            if (nlf_[i].find(label) == nlf_[i].end()) {
                nlf_[i][label] = 0;
            }

            nlf_[i][label] += 1;
        }
    }
}

void Graph::BuildLabelOffset() {
    size_t labels_offset_size = (size_t)vertices_count_ * labels_count_ + 1;
    labels_offsets_ = new ui[labels_offset_size];
    std::fill(labels_offsets_, labels_offsets_ + labels_offset_size, 0);

    for (ui i = 0; i < vertices_count_; ++i) {
        std::sort(neighbors_ + offsets_[i], neighbors_ + offsets_[i + 1],
            [this](const VertexID u, const VertexID v) -> bool {
                return labels_[u] == labels_[v] ? u < v : labels_[u] < labels_[v];
            });
    }

    for (ui i = 0; i < vertices_count_; ++i) {
        LabelID previous_label = 0;
        LabelID current_label = 0;

        labels_offset_size = i * labels_count_;
        labels_offsets_[labels_offset_size] = offsets_[i];

        for (ui j = offsets_[i]; j < offsets_[i + 1]; ++j) {
            current_label = labels_[neighbors_[j]];

            if (current_label != previous_label) {
                for (ui k = previous_label + 1; k <= current_label; ++k) {
                    labels_offsets_[labels_offset_size + k] = j;
                }
                previous_label = current_label;
            }
        }

        for (ui l = current_label + 1; l <= labels_count_; ++l) {
            labels_offsets_[labels_offset_size + l] = offsets_[i + 1];
        }
    }
}

#endif

void Graph::loadGraphFromFile(const std::string &file_path) {
    std::ifstream infile(file_path);

    if (!infile.is_open()) {
        std::cout << "Can not open the graph file " << file_path << " ." << std::endl;
        exit(-1);
    }

    char type;
    infile >> type >> vertices_count_ >> edges_count_;
    //offsets储存每个节点到邻接表之间的偏移
    offsets_ = new ui[vertices_count_ +  1];
    offsets_[0] = 0;
    //neibors储存整个邻接表，通过offset寻找
    neighbors_ = new VertexID[edges_count_ * 2];
    labels_ = new LabelID[vertices_count_];
    labels_count_ = 0;
    max_degree_ = 0;

    LabelID max_label_id = 0;
    //neighors_offset从0递增，储存每个结点的邻居offset
    std::vector<ui> neighbors_offset(vertices_count_, 0);

    while (infile >> type) {
        if (type == 'v') { // Read vertex.
            VertexID id;
            LabelID  label;
            ui degree;
            infile >> id >> label >> degree;

            labels_[id] = label;
            offsets_[id + 1] = offsets_[id] + degree;

            if (degree > max_degree_) {
                max_degree_ = degree;
            }

            if (labels_frequency_.find(label) == labels_frequency_.end()) {
                labels_frequency_[label] = 0;
                if (label > max_label_id)
                    max_label_id = label;
            }

            labels_frequency_[label] += 1;
        }
        else if (type == 'e') { // Read edge.
            VertexID begin;
            VertexID end;
            infile >> begin >> end;

            ui offset = offsets_[begin] + neighbors_offset[begin];
            neighbors_[offset] = end;

            offset = offsets_[end] + neighbors_offset[end];
            neighbors_[offset] = begin;

            neighbors_offset[begin] += 1;
            neighbors_offset[end] += 1;
        }
    }

    infile.close();
    labels_count_ = (ui)labels_frequency_.size() > (max_label_id + 1) ? (ui)labels_frequency_.size() : max_label_id + 1;

    //计算最大的标签频数
    for (auto element : labels_frequency_) {
        if (element.second > max_label_frequency_) {
            max_label_frequency_ = element.second;
        }
    }

    //对每个结点的邻居进行排序
    for (ui i = 0; i < vertices_count_; ++i) {
        std::sort(neighbors_ + offsets_[i], neighbors_ + offsets_[i + 1]);
    }

    //建立由标签到结点的逆向索引
    BuildReverseIndex();

#if OPTIMIZED_LABELED_GRAPH == 1
    if (enable_label_offset_) {
        BuildNLF();
        // BuildLabelOffset();
    }
#endif
}

void Graph::printGraphMetaData() {
    std::cout << "|V|: " << vertices_count_ << ", |E|: " << edges_count_ << ", |\u03A3|: " << labels_count_ << std::endl;
    std::cout << "Max Degree: " << max_degree_ << ", Max Label Frequency: " << max_label_frequency_ << std::endl;
}

void Graph::loadGraphFromFileCompressed(const std::string &degree_path, const std::string &edge_path,
                                        const std::string &label_path) {
    std::ifstream deg_file(degree_path, std::ios::binary);

    if (deg_file.is_open()) {
        std::cout << "Open degree file " << degree_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot open degree file " << degree_path << " ." << std::endl;
        exit(-1);
    }

    auto start = std::chrono::high_resolution_clock::now();
    int int_size;
    deg_file.read(reinterpret_cast<char *>(&int_size), 4);
    deg_file.read(reinterpret_cast<char *>(&vertices_count_), 4);
    deg_file.read(reinterpret_cast<char *>(&edges_count_), 4);

    offsets_ = new ui[vertices_count_ + 1];
    ui* degrees = new unsigned int[vertices_count_];

    deg_file.read(reinterpret_cast<char *>(degrees), sizeof(int) * vertices_count_);


    deg_file.close();
    deg_file.clear();

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Load degree file time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " seconds" << std::endl;

    std::ifstream adj_file(edge_path, std::ios::binary);

    if (adj_file.is_open()) {
        std::cout << "Open edge file " << edge_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot open edge file " << edge_path << " ." << std::endl;
        exit(-1);
    }

    start = std::chrono::high_resolution_clock::now();
    size_t neighbors_count = (size_t)edges_count_ * 2;
    neighbors_ = new ui[neighbors_count];

    offsets_[0] = 0;
    for (ui i = 1; i <= vertices_count_; ++i) {
        offsets_[i] = offsets_[i - 1] + degrees[i - 1];
    }

    max_degree_ = 0;

    for (ui i = 0; i < vertices_count_; ++i) {
        if (degrees[i] > 0) {
            if (degrees[i] > max_degree_)
                max_degree_ = degrees[i];
            adj_file.read(reinterpret_cast<char *>(neighbors_ + offsets_[i]), degrees[i] * sizeof(int));
            std::sort(neighbors_ + offsets_[i], neighbors_ + offsets_[i + 1]);
        }
    }

    adj_file.close();
    adj_file.clear();

    delete[] degrees;

    end = std::chrono::high_resolution_clock::now();
    std::cout << "Load adj file time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " seconds" << std::endl;


    std::ifstream label_file(label_path, std::ios::binary);
    if (label_file.is_open())  {
        std::cout << "Open label file " << label_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot open label file " << label_path << " ." << std::endl;
        exit(-1);
    }

    start = std::chrono::high_resolution_clock::now();

    labels_ = new ui[vertices_count_];
    label_file.read(reinterpret_cast<char *>(labels_), sizeof(int) * vertices_count_);

    label_file.close();
    label_file.clear();

    ui max_label_id = 0;
    for (ui i = 0; i < vertices_count_; ++i) {
        ui label = labels_[i];

        if (labels_frequency_.find(label) == labels_frequency_.end()) {
            labels_frequency_[label] = 0;
            if (label > max_label_id)
                max_label_id = label;
        }

        labels_frequency_[label] += 1;
    }

    labels_count_ = (ui)labels_frequency_.size() > (max_label_id + 1) ? (ui)labels_frequency_.size() : max_label_id + 1;

    for (auto element : labels_frequency_) {
        if (element.second > max_label_frequency_) {
            max_label_frequency_ = element.second;
        }
    }

    end = std::chrono::high_resolution_clock::now();
    std::cout << "Load label file time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " seconds" << std::endl;

    start = std::chrono::high_resolution_clock::now();
    BuildReverseIndex();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Build reverse index file time: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << " seconds" << std::endl;
#if OPTIMIZED_LABELED_GRAPH == 1
    if (enable_label_offset_) {
        BuildNLF();
        // BuildLabelOffset();
    }
#endif
}

void Graph::storeComparessedGraph(const std::string& degree_path, const std::string& edge_path,
                                  const std::string& label_path) {
    ui* degrees = new ui[vertices_count_];
    for (ui i = 0; i < vertices_count_; ++i) {
        degrees[i] = offsets_[i + 1] - offsets_[i];
    }

    std::ofstream deg_outputfile(degree_path, std::ios::binary);

    if (deg_outputfile.is_open()) {
        std::cout << "Open degree file " << degree_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot degree edge file " << degree_path << " ." << std::endl;
        exit(-1);
    }

    int int_size = sizeof(int);
    size_t vertex_array_bytes = ((size_t)vertices_count_) * 4;
    deg_outputfile.write(reinterpret_cast<const char *>(&int_size), 4);
    deg_outputfile.write(reinterpret_cast<const char *>(&vertices_count_), 4);
    deg_outputfile.write(reinterpret_cast<const char *>(&edges_count_), 4);
    deg_outputfile.write(reinterpret_cast<const char *>(degrees), vertex_array_bytes);

    deg_outputfile.close();
    deg_outputfile.clear();

    delete[] degrees;

    std::ofstream edge_outputfile(edge_path, std::ios::binary);

    if (edge_outputfile.is_open()) {
        std::cout << "Open edge file " << edge_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot edge file " << edge_path << " ." << std::endl;
        exit(-1);
    }

    size_t edge_array_bytes = ((size_t)edges_count_ * 2) * 4;
    edge_outputfile.write(reinterpret_cast<const char *>(neighbors_), edge_array_bytes);

    edge_outputfile.close();
    edge_outputfile.clear();

    std::ofstream label_outputfile(label_path, std::ios::binary);

    if (label_outputfile.is_open()) {
        std::cout << "Open label file " << label_path << " successfully." << std::endl;
    }
    else {
        std::cerr << "Cannot label file " << label_path << " ." << std::endl;
        exit(-1);
    }

    size_t label_array_bytes = ((size_t)vertices_count_) * 4;
    label_outputfile.write(reinterpret_cast<const char *>(labels_), label_array_bytes);

    label_outputfile.close();
    label_outputfile.clear();
}

void Graph::buildCoreTable() {
    core_table_ = new int[vertices_count_];

    int vertices_count = getVerticesCount();
    int max_degree = getGraphMaxDegree();

    int* vertices = new int[vertices_count];          // Vertices sorted by degree.
    int* position = new int[vertices_count];          // The position of vertices in vertices array.
    int* degree_bin = new int[max_degree + 1];      // Degree from 0 to max_degree.
    int* offset = new int[max_degree + 1];          // The offset in vertices array according to degree.

    std::fill(degree_bin, degree_bin + (max_degree + 1), 0);

    for (int i = 0; i < vertices_count; ++i) {
        int degree = getVertexDegree(i);
        core_table_[i] = degree;
        degree_bin[degree] += 1;
    }

    int start = 0;
    for (int i = 0; i < max_degree + 1; ++i) {
        offset[i] = start;
        start += degree_bin[i];
    }

    for (int i = 0; i < vertices_count; ++i) {
        int degree = getVertexDegree(i);
        position[i] = offset[degree];
        vertices[position[i]] = i;
        offset[degree] += 1;
    }

    for (int i = max_degree; i > 0; --i) {
        offset[i] = offset[i - 1];
    }
    offset[0] = 0;

    for (int i = 0; i < vertices_count; ++i) {
        int v = vertices[i];

        ui count;
        const VertexID * neighbors = getVertexNeighbors(v, count);

        for(int j = 0; j < count; ++j) {
            int u = neighbors[j];

            if (core_table_[u] > core_table_[v]) {

                // Get the position and vertex which is with the same degree
                // and at the start position of vertices array.
                int cur_degree_u = core_table_[u];
                int position_u = position[u];
                int position_w = offset[cur_degree_u];
                int w = vertices[position_w];

                if (u != w) {
                    // Swap u and w.
                    position[u] = position_w;
                    position[w] = position_u;
                    vertices[position_u] = w;
                    vertices[position_w] = u;
                }

                offset[cur_degree_u] += 1;
                core_table_[u] -= 1;
            }
        }
    }

    delete[] vertices;
    delete[] position;
    delete[] degree_bin;
    delete[] offset;
    for (ui i = 0; i < vertices_count_; ++i) {
        if (core_table_[i] > 1) {
            core_length_ += 1;
        }
    }
}

#endif //SUBGRAPHMATCHING_GRAPH_H