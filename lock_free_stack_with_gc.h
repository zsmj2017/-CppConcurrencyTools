#ifndef LOCK_FREE_STACK_WITH_GC_H
#define LOCK_FREE_STACK_WITH_GC_H

template<typename T>
class lock_free_stack {
private:
    struct node {
        std::shared_ptr<T> data;
        node *next;

        explicit node(T const &data_) : data(std::make_shared<T>(data_)) {}
    };

    std::atomic<node *> head;
public:
    lock_free_stack() = default;
    ~lock_free_stack() {
        delete_nodes(head);
    }
public:
    void push(T const &data) {
        node *const new_node = new node(data);
        new_node->next = head.load();
        while (!head.compare_exchange_weak(new_node->next, new_node));
    }

private: // GC
    std::atomic<unsigned> threads_in_pop{};
    std::atomic<node *> to_be_deleted;

    static void delete_nodes(node *nodes) {
        while (nodes) {
            node *next = nodes->next;
            delete nodes;
            nodes = next;
        }
    }

    void try_reclaim(node *old_head) {
        if (threads_in_pop == 1) {
            node *nodes_to_delete = to_be_deleted.exchange(nullptr);
            if (0 == --threads_in_pop) { // 3
                delete_nodes(nodes_to_delete);
            } else if (nodes_to_delete) {
                chain_pending_nodes(nodes_to_delete); // 2
            }
            delete old_head; // 1
        } else {
            chain_pending_node(old_head);
            --threads_in_pop;
        }
    }

    void chain_pending_nodes(node *nodes) {
        // 获取链表尾部
        node *last = nodes;
        while (node * const next = last->next) {
            last = next;
        }
        chain_pending_nodes(nodes, last);
    }

    // 更新待删除列表
    void chain_pending_nodes(node *first, node *last) {
        last->next = to_be_deleted;
        while (!to_be_deleted.compare_exchange_weak(last->next, first));
    }

    void chain_pending_node(node *n) {
        chain_pending_nodes(n, n);
    }
public:
    std::shared_ptr<T> pop() {
        ++threads_in_pop; // 增加计数
        node *old_head = head.load();
        while (old_head && !head.compare_exchange_weak(old_head, old_head->next));
        std::shared_ptr<T> res;
        if (old_head) {
            res.swap(old_head->data); // 交换而非拷贝
        }
        try_reclaim(old_head); // 尝试回收删除的节点，递减计数
        return res;
    }
};

#endif //LOCK_FREE_STACK_WITH_GC_H
