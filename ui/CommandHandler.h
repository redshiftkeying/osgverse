#ifndef MANA_UI_COMMANDHANDLER_HPP
#define MANA_UI_COMMANDHANDLER_HPP

#include <osg/Camera>
#include <osg/MatrixTransform>
#include <osgGA/GUIEventHandler>
#include <mutex>
#include <any>

namespace osgVerse
{
    enum CommandType
    {
        CommandToScene = 0,
        SetNodeCommand,          // TODO: [node]parent, [node]item-added, [bool]to-delete
        SetValueCommand,         // TODO: [node]item, [string]key, [any]value
        TransformCommand,        // TODO: [node]item, [matrix]transformation
        LoadModelCommand,        // [node]parent, [string]url

        CommandToUI = 100,
        RefreshHierarchy,        // [node]parent, [node]item-added, [bool]to-delete (TODO)
        RefreshHierarchyItem,    // [node]item, [string]value-type
        RefreshProperties,       // [node]item, [string]component-name
    };

    struct CommandData
    {
        osg::observer_ptr<osg::Object> object;
        std::any value, valueEx; CommandType type;

        template<typename T> bool get(T& v, int i = 0)
        {
            try { v = std::any_cast<T>(i == 0 ? value : valueEx); return true; }
            catch (std::bad_any_cast&) {} return false;
        }
    };

    class CommandBuffer : public osg::Referenced
    {
    public:
        static CommandBuffer* instance();
        bool canMerge(const std::list<CommandData>& cList, CommandType t, osg::Object* n) const;
        void mergeCommand(std::list<CommandData>& cList, CommandType t, osg::Object* n,
                          const std::any& v0, const std::any& v1);

        void add(CommandType t, osg::Object* n, const std::any& v0, const std::any& v1 = (int)0);
        bool take(CommandData& c, bool fromSceneHandler);

    protected:
        CommandBuffer() {}
        std::list<CommandData> _bufferToScene;
        std::list<CommandData> _bufferToUI;
        std::mutex _mutex;
    };

    class CommandHandler : public osgGA::GUIEventHandler
    {
    public:
        CommandHandler();
        virtual bool handle(const osgGA::GUIEventAdapter& ea, osgGA::GUIActionAdapter& aa);

        struct CommandExecutor : public osg::Referenced
        {
            virtual bool redo(CommandData& cmd) = 0;
            virtual bool undo(CommandData& cmd) { return false; }
        };
        void addExecutor(CommandType t, CommandExecutor* e) { _executors[t] = e; }
        CommandExecutor* getExecutor(CommandType t) { return _executors[t].get(); }

    protected:
        std::map<CommandType, osg::ref_ptr<CommandExecutor>> _executors;
    };
}

#endif