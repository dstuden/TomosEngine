#pragma once
#include "Tomos/core/TNode.hh"

namespace Tomos
{
    class Script
    {
    public:
        virtual ~Script() = default;

        virtual void earlyUpdate()
        {
        }

        virtual void update()
        {
        }

        virtual void lateUpdate()
        {
        }

        virtual void onAttach()
        {
        }

        virtual void onDetach()
        {
        }

        std::shared_ptr<TNode> m_node;
    };

    class TScriptComponent : public TComponent
    {
    public:
        TScriptComponent( const std::shared_ptr<Script>& p_script, const std::string& p_name = "UnnamedScriptComponent" )
        {
            m_name   = p_name;
            m_script = p_script;
        }

        const std::shared_ptr<Script>& getScript() { return m_script; }

    private:
        std::shared_ptr<Script> m_script;
    };
} // Tomos
