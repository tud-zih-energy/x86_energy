/**
 * x86_energy.hpp
 *
 *  Created on: 26.06.2018
 *      Author: bmario
 */

#ifndef INCLUDE_X86_ENERGY_HPP_
#define INCLUDE_X86_ENERGY_HPP_

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include <cstdint>

extern "C"
{
#include <x86_energy.h>
}

namespace x86_energy
{

enum class Granularity
{
    SYSTEM = X86_ENERGY_GRANULARITY_SYSTEM,
    SOCKET = X86_ENERGY_GRANULARITY_SOCKET,
    DIE = X86_ENERGY_GRANULARITY_DIE,
    MODULE = X86_ENERGY_GRANULARITY_MODULE,
    CORE = X86_ENERGY_GRANULARITY_CORE,
    THREAD = X86_ENERGY_GRANULARITY_THREAD,
    DEVICE = X86_ENERGY_GRANULARITY_DEVICE,
    SIZE = X86_ENERGY_GRANULARITY_SIZE
};

inline std::ostream& operator<<(std::ostream& s, Granularity g)
{
    switch (g)
    {
    case Granularity::SYSTEM:
        s << "SYSTEM";
        break;
    case Granularity::SOCKET:
        s << "SOCKET";
        break;
    case Granularity::DIE:
        s << "DIE";
        break;
    case Granularity::MODULE:
        s << "MODULE";
        break;
    case Granularity::CORE:
        s << "CORE";
        break;
    case Granularity::THREAD:
        s << "THREAD";
        break;
    case Granularity::DEVICE:
        s << "DEVICE";
        break;
    default:
        s << "INVALID";
        break;
    }
    return s;
}

/**
 * Enum for different types of energy counters
 */
enum class Counter
{
    PCKG = X86_ENERGY_COUNTER_PCKG,
    CORES = X86_ENERGY_COUNTER_CORES,
    DRAM = X86_ENERGY_COUNTER_DRAM,
    GPU = X86_ENERGY_COUNTER_GPU,
    PLATFORM = X86_ENERGY_COUNTER_PLATFORM,
    SINGLE_CORE = X86_ENERGY_COUNTER_SINGLE_CORE,
    SIZE = X86_ENERGY_COUNTER_SIZE
};

inline std::ostream& operator<<(std::ostream& s, Counter c)
{
    switch (c)
    {
    case Counter::PCKG:
        s << "PCKG";
        break;
    case Counter::CORES:
        s << "CORES";
        break;
    case Counter::DRAM:
        s << "DRAM";
        break;
    case Counter::GPU:
        s << "GPU";
        break;
    case Counter::PLATFORM:
        s << "PLATFORM";
        break;
    case Counter::SINGLE_CORE:
        s << "SINGLE_CORE";
        break;
    default:
        s << "INVALID";
        break;
    }

    return s;
}

class Architecture;

class ArchitectureNode
{
private:
    ArchitectureNode() = default;

    ArchitectureNode(x86_energy_architecture_node_t* node) : node_(node)
    {
        initialize_children();
    }

    void initialize_children()
    {
        for (auto i = 0u; i < node_->nr_children; i++)
        {
            children_.emplace_back(ArchitectureNode(&node_->children[i]));
        }
    }

public:
    Granularity granularity() const
    {
        return static_cast<Granularity>(node_->granularity);
    }

    std::int32_t id() const
    {
        return node_->id;
    }

    std::string name() const
    {
        return node_->name;
    }

    const std::vector<ArchitectureNode>& children() const
    {
        return children_;
    }

    friend class Architecture;

private:
    x86_energy_architecture_node_t* node_;
    std::vector<ArchitectureNode> children_;
};

class Architecture : public ArchitectureNode
{
    struct ArchitectureNodeDeleter
    {
        void operator()(x86_energy_architecture_node_t* p) const
        {
            x86_energy_free_architecture_nodes(p);
        }
    };

public:
    Architecture() : root_node_(x86_energy_init_architecture_nodes())
    {
        node_ = root_node_.get();
        if (node_ == nullptr)
        {
            throw std::runtime_error(x86_energy_error_string());
        }

        initialize_children();
    }

    ArchitectureNode get_arch_for_cpu(Granularity granularity, int cpu) const
    {
        auto node = x86_energy_find_arch_for_cpu(
            root_node_.get(), static_cast<x86_energy_granularity>(granularity), cpu);

        if (node == nullptr)
        {
            throw std::runtime_error(x86_energy_error_string());
        }

        return ArchitectureNode(node);
    }

    int size(Granularity granularity) const
    {
        return x86_energy_arch_count(root_node_.get(),
                                     static_cast<x86_energy_granularity>(granularity));
    }

    void print_tree(int lvl = 0) const
    {
        x86_energy_print(root_node_.get(), lvl);
    }

private:
    std::unique_ptr<x86_energy_architecture_node_t, ArchitectureNodeDeleter> root_node_;
};

class SourceCounter
{
public:
    SourceCounter() = delete;

    SourceCounter(x86_energy_access_source_t* source, x86_energy_single_counter_t source_counter)
    : source_(source), source_counter_(source_counter)
    {
        if (source_ == nullptr || source_counter_ == nullptr)
        {
            throw std::runtime_error("Trying to construct a source counter with invalid values.");
        }
    }

    ~SourceCounter()
    {
        if (source_)
        {
            source_->close(source_counter_);
        }
    }

    SourceCounter(const SourceCounter&) = delete;
    SourceCounter& operator=(const SourceCounter&) = delete;

    SourceCounter(SourceCounter&& other)
    : source_(other.source_), source_counter_(other.source_counter_)
    {
        other.source_ = nullptr;
    }

    SourceCounter& operator=(SourceCounter&& other)
    {
        std::swap(source_, other.source_);
        std::swap(source_counter_, other.source_counter_);

        return *this;
    }

public:
    double read()
    {
        auto result = source_->read(source_counter_);

        if (result < 0)
        {
            throw std::runtime_error("Couldn't read from source counter");
        }

        return result;
    }

private:
    x86_energy_access_source_t* source_;
    x86_energy_single_counter_t source_counter_;
};

class AccessSource
{
public:
    AccessSource() = delete;

    AccessSource(x86_energy_access_source_t* source) : source_(source)
    {
        if (source_ == nullptr)
        {
            throw std::runtime_error("Trying to construct an access source from a null pointer.");
        }
    }

    ~AccessSource()
    {
        if (initialized_)
        {
            source_->fini();
        }
    }

    AccessSource(const AccessSource&) = delete;
    AccessSource& operator=(const AccessSource&) = delete;

    AccessSource(AccessSource&& other) : source_(other.source_), initialized_(other.initialized_)
    {
        other.source_ = nullptr;
        other.initialized_ = false;
    }

    AccessSource& operator=(AccessSource&& other)
    {
        std::swap(source_, other.source_);
        std::swap(initialized_, other.initialized_);

        return *this;
    }

public:
    void init()
    {
        int result = source_->init();
        if (result != 0)
        {
            throw std::system_error(result, std::system_category());
        }

        initialized_ = true;
    }

    std::string name() const
    {
        return source_->name;
    }

    SourceCounter get(Counter counter, std::size_t index)
    {
        if (!initialized_)
        {
            throw std::runtime_error("Trying to use an uninitialized access source");
        }

        x86_energy_single_counter_t result =
            source_->setup(static_cast<x86_energy_counter>(counter), index);
        if (result == nullptr)
        {
            throw std::runtime_error(x86_energy_error_string());
        }
        return { source_, result };
    }

private:
    x86_energy_access_source_t* source_;
    bool initialized_ = false;
};

class Mechanism
{
public:
    Mechanism() : mechanism_(x86_energy_get_avail_mechanism())
    {
        if (mechanism_ == nullptr)
        {
            throw std::runtime_error(x86_energy_error_string());
        }
    }

public:
    std::string name() const
    {
        return mechanism_->name;
    }

    Granularity granularity(Counter counter) const
    {
        return static_cast<Granularity>(
            mechanism_->source_granularities[static_cast<x86_energy_counter>(counter)]);
    }

    std::vector<AccessSource> available_sources() const
    {
        std::vector<AccessSource> result;

        for (size_t i = 0; i < mechanism_->nr_avail_sources; i++)
        {
            result.emplace_back(&mechanism_->avail_sources[i]);
        }

        return result;
    }

private:
    x86_energy_mechanisms_t* mechanism_;
};

} // namespace x86_energy
#endif /* INCLUDE_X86_ENERGY_HPP_ */
