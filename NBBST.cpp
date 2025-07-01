#ifndef NBBST_TREE
#define NBBST_TREE

#include <cassert>
#include <functional>
#include <limits>
#include <stack>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <vector>

#ifndef HASH
#define HASH

/*!
 * Hash the given value depending on its type.
 * This function is made to be template-specialized.
 * \param T The type of value to hash.
 * \param value The value to hash.
 */
template <typename T>
int hash(T value);

/*!
 * Specialization of hash for the int type.
 */
template <>
inline int hash<int>(int value)
{
    return value;
}

#endif

#ifndef UTILS
#define UTILS

/*!
 * Compare and Swap a pointer.
 * \param ptr The pointer to swap.
 * \param old The expected value.
 * \param value The new value to set.
 * \return true if the CAS suceeded, otherwise false.
 */
template <typename T>
bool inline CASPTR(T **ptr, T *old, T *value)
{
    return __sync_bool_compare_and_swap(ptr, old, value);
}

#endif

namespace nbbst
{

    enum UpdateState
    {
        CLEAN = 0,
        DFLAG = 1,
        IFLAG = 2,
        MARK = 3
    };

    struct Node;

    struct Info;
    typedef Info *Update;

    struct Info
    {
        Node *gp;          // Internal
        Node *p;           // Internal
        Node *newInternal; // Internal
        Node *l;           // Leaf
        Update pupdate;

        Info()
            : gp(nullptr),
              p(nullptr),
              newInternal(nullptr),
              l(nullptr),
              pupdate(nullptr) {}
    };

    typedef Info *Update;

    inline UpdateState getState(Update update)
    {
        return static_cast<UpdateState>(reinterpret_cast<unsigned long>(update) &
                                        3l);
    }

    inline Update Unmark(Update info)
    {
        return reinterpret_cast<Update>(reinterpret_cast<unsigned long>(info) &
                                        (~0l - 3));
    }

    inline Update Mark(Update info, UpdateState state)
    {
        return reinterpret_cast<Update>(
            (reinterpret_cast<unsigned long>(info) & (~0l - 3)) |
            static_cast<unsigned int>(state));
    }

    struct Version
    {
        int key;
        Version *l;
        Version *r;
        int sum;
        Version(int key) : key(key), l(nullptr), r(nullptr), sum(1) {};
    };

    struct Node
    {
        bool internal;
        int key;

        Update update;
        Node *left;
        Node *right;
        Version *version;
        Node()
            : internal(internal),
              key(key),
              update(nullptr),
              left(nullptr),
              right(nullptr),
              version(nullptr) {};
    };

    struct SearchResult
    {
        Node *gp; // Internal
        Node *p;  // Internal
        Node *l;  // Leaf
        Update pupdate;
        Update gpupdate;

        SearchResult()
            : gp(nullptr),
              p(nullptr),
              l(nullptr),
              pupdate(nullptr),
              gpupdate(nullptr) {}
    };

    bool Refresh(Node *x)
    {
        Version *old = x->version;
        Version *vl, *vr;
        Node *xr, *xl;

        do
        {
            xr = x->right;
            vr = xr->version;
        } while (xr != x->right);
        do
        {
            xl = x->left;
            vl = xl->version;
        } while (xl != x->left);
        Version *newVersion = new Version(x->key);
        newVersion->l = vl;
        newVersion->r = vr;
        newVersion->sum = vl->sum + vr->sum;
        return CASPTR(&x->version, old, newVersion);
    }

    void Propagate(std::stack<Node *> bt)
    {
        bt.pop();
        while (!bt.empty())
        {
            Node *x = bt.top();
            bt.pop();
            if (!Refresh(x))
                Refresh(x);
        }
    }

    template <typename T, int Threads>
    class NBBST
    {
    public:
        NBBST();
        ~NBBST();

        bool contains(T value);
        bool add(T value);
        bool remove(T value);
        int size() { return root->version ? root->version->sum : 0; }

    private:
        void Search(int key, SearchResult *result, std::stack<Node *> *bt);
        void HelpInsert(Info *op);
        bool HelpDelete(Info *op);
        void HelpMarked(Info *op);
        void Help(Update u);
        void CASChild(Node *parent, Node *old, Node *newNode);

        /* Allocate stuff from the hazard manager  */
        Node *newInternal(int key);
        Node *newLeaf(int key);
        Info *newIInfo(Node *p, Node *newInternal, Node *l);
        Info *newDInfo(Node *gp, Node *p, Node *l, Update pupdate);

        /* To remove properly a node  */
        void releaseNode(Node *node);

        Node *root;
    };

    template <typename T, int Threads>
    void NBBST<T, Threads>::releaseNode(Node *node)
    {
        if (node)
        {
            if (node->update)
            {
                Unmark(node->update);
            }

            // nodes.releaseNode(node);
        }
    }

    template <typename T, int Threads>
    NBBST<T, Threads>::NBBST()
    {
        root = newInternal(std::numeric_limits<int>::max());
        root->update = Mark(nullptr, CLEAN);

        root->left = newLeaf(std::numeric_limits<int>::min());
        root->right = newLeaf(std::numeric_limits<int>::max());
        root->left->version->sum = 0;
        root->right->version->sum = 0;
        root->version->sum = 0;
    }

    template <typename T, int Threads>
    NBBST<T, Threads>::~NBBST() {}

    template <typename T, int Threads>
    Node *NBBST<T, Threads>::newInternal(int key)
    {
        Node *node = new Node();

        node->internal = true;
        node->key = key;
        node->version = new Version(key);

        return node;
    }

    template <typename T, int Threads>
    Node *NBBST<T, Threads>::newLeaf(int key)
    {
        Node *node = new Node();

        node->internal = false;
        node->key = key;
        node->version = new Version(key);
        return node;
    }

    template <typename T, int Threads>
    Info *NBBST<T, Threads>::newIInfo(Node *p, Node *newInternal, Node *l)
    {
        Info *info = new Info();

        info->p = p;
        info->newInternal = newInternal;
        info->l = l;

        return info;
    }

    template <typename T, int Threads>
    Info *NBBST<T, Threads>::newDInfo(Node *gp, Node *p, Node *l, Update pupdate)
    {
        Info *info = new Info();

        info->gp = gp;
        info->p = p;
        info->l = l;
        info->pupdate = pupdate;

        return info;
    }

    template <typename T, int Threads>
    void NBBST<T, Threads>::Search(int key, SearchResult *result,
                                   std::stack<Node *> *bt)
    {
        Node *l = root;
        bt->push(l);
        while (l->internal)
        {
            result->gp = result->p;
            result->p = l;
            result->gpupdate = result->pupdate;
            result->pupdate = result->p->update;

            if (key < l->key)
            {
                l = result->p->left;
            }
            else
            {
                l = result->p->right;
            }
            bt->push(l);
        }

        result->l = l;
    }

    template <typename T, int Threads>
    bool NBBST<T, Threads>::contains(T value)
    {
        int key = hash(value);
        std::stack<Node *> bt;
        SearchResult result;
        Search(key, &result, &bt);

        return result.l->key == key;
    }

    template <typename T, int Threads>
    bool NBBST<T, Threads>::add(T value)
    {
        int key = hash(value);
        Node *newNode = newLeaf(key);
        SearchResult search;

        while (true)
        {
            std::stack<Node *> bt;
            Search(key, &search, &bt);
            if (search.l->key == key)
            {
                Propagate(bt);
                return false; // Key already in the set
            }
            if (getState(search.pupdate) != CLEAN)
            {
                Help(search.pupdate);
            }
            else
            {
                Version *sibling = search.l->version;
                Node *newSibling = newLeaf(search.l->key);
                newSibling->version->sum = sibling->sum;
                Node *newInt = newInternal(std::max(key, search.l->key));
                newInt->update = Mark(nullptr, CLEAN);
                newInt->version = new Version(newInt->key);
                newInt->version->sum =
                    newNode->version->sum + newSibling->version->sum;
                // Put the smaller child on the left
                if (newNode->key <= newSibling->key)
                {
                    newInt->left = newNode;
                    newInt->right = newSibling;
                    newInt->version->l = newNode->version;
                    newInt->version->r = newSibling->version;
                }
                else
                {
                    newInt->left = newSibling;
                    newInt->right = newNode;
                    newInt->version->r = newNode->version;
                    newInt->version->l = newSibling->version;
                }
                Info *op = newIInfo(search.p, newInt, search.l);
                Update result = search.p->update;
                if (CASPTR(&search.p->update, search.pupdate, Mark(op, IFLAG)))
                {
                    HelpInsert(op);
                    if (search.pupdate)
                    {
                        (Unmark(search.pupdate));
                    }
                    Propagate(bt);
                    return true;
                }
                else
                {
                    Help(result);
                }
            }
        }
    }

    template <typename T, int Threads>
    bool NBBST<T, Threads>::remove(T value)
    {
        int key = hash(value);
        SearchResult search;
        while (true)
        {
            std::stack<Node *> bt;
            Search(key, &search, &bt);
            if (search.l->key != key)
            {
                Propagate(bt);
                return false;
            }
            if (getState(search.gpupdate) != CLEAN)
            {
                Help(search.gpupdate);
            }
            else if (getState(search.pupdate) != CLEAN)
            {
                Help(search.pupdate);
            }
            else
            {
                Info *op = newDInfo(search.gp, search.p, search.l, search.pupdate);
                Update result = search.gp->update;
                if (CASPTR(&search.gp->update, search.gpupdate, Mark(op, DFLAG)))
                {
                    if (search.gpupdate)
                    {
                        (Unmark(search.gpupdate));
                    }
                    if (HelpDelete(op))
                    {
                        Propagate(bt);
                        return true;
                    }
                }
                else
                {
                    Help(result);
                }
            }
        }
    }

    template <typename T, int Threads>
    void NBBST<T, Threads>::Help(Update u)
    {
        if (getState(u) == IFLAG)
        {
            HelpInsert(Unmark(u));
        }
        else if (getState(u) == MARK)
        {
            HelpMarked(Unmark(u));
        }
        else if (getState(u) == DFLAG)
        {
            HelpDelete(Unmark(u));
        }
    }

    template <typename T, int Threads>
    void NBBST<T, Threads>::HelpInsert(Info *op)
    {
        CASChild(op->p, op->l, op->newInternal);
        CASPTR(&op->p->update, Mark(op, IFLAG), Mark(op, CLEAN));
    }

    template <typename T, int Threads>
    bool NBBST<T, Threads>::HelpDelete(Info *op)
    {
        Update result = op->p->update;
        // If we succeed
        if (CASPTR(&op->p->update, op->pupdate, Mark(op, MARK)))
        {
            if (op->pupdate)
            {
                Unmark(op->pupdate);
            }
            HelpMarked(Unmark(op));
            return true;
        }
        // if another has succeeded for us
        else if (getState(op->p->update) == MARK &&
                 Unmark(op->p->update) == Unmark(op))
        {
            HelpMarked(Unmark(op));
            return true;
        }
        else
        {
            Help(result);
            CASPTR(&op->gp->update, Mark(op, DFLAG), Mark(op, CLEAN));
            return false;
        }
    }

    template <typename T, int Threads>
    void NBBST<T, Threads>::HelpMarked(Info *op)
    {
        Node *other;
        if (op->p->right == op->l)
        {
            other = op->p->left;
        }
        else
        {
            other = op->p->right;
        }
        CASChild(op->gp, op->p, other);
        CASPTR(&op->gp->update, Mark(op, DFLAG), Mark(op, CLEAN));
    }

    template <typename T, int Threads>
    void NBBST<T, Threads>::CASChild(Node *parent, Node *old, Node *newNode)
    {
        if (newNode->key < parent->key)
        {
            if (CASPTR(&parent->left, old, newNode))
            {
                if (old)
                {
                }
            }
        }
        else
        {
            if (CASPTR(&parent->right, old, newNode))
            {
                if (old)
                {
                }
            }
        }
    }

} // namespace nbbst

int main(int argc, char const *argv[])
{
    nbbst::NBBST<int, 3> tree;

    // Lambda for adding elements
    auto add_values = [&tree](const std::vector<int> &values)
    {
        for (int val : values)
        {
            tree.add(val);
        }
    };

    auto remove_values = [&tree](const std::vector<int> &values)
    {
        for (int val : values)
        {
            tree.remove(val);
        }
    };

    // Two sets of values for two threads
    std::vector<int> values1 = {5, 10, 3, 0, 2, 7};
    std::vector<int> values2 = {8, 4, 6, 11, 1, 9};

    // Create threads

    std::thread t1(add_values, values1);
    std::thread t3(remove_values, values1);
    std::thread t2(add_values, values2);

    // Wait for both threads to finish
    t1.join();
    t2.join();
    t3.join();
    // Print final size
    std::cout << "Final Size: " << tree.size() << std::endl;

    return 0;
}

#endif
