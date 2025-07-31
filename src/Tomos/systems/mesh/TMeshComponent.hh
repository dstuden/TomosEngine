#pragma once
#include <memory>

#include "../../util/renderer/TMaterial.hh"
#include "TMesh.hh"
#include "Tomos/systems/TComponent.hh"

namespace Tomos
{
    class TMeshComponent : public TComponent
    {
    public:
        TMeshComponent( const std::shared_ptr<TMesh>& p_mesh, const std::shared_ptr<TMaterial>& p_material, const std::string& p_name = "MeshComponent" )
        {
            m_material = p_material;
            m_mesh     = p_mesh;
            m_name     = p_name;
        }

        std::shared_ptr<TMesh>     getMesh() const { return m_mesh; }
        std::shared_ptr<TMaterial> getMaterial() const { return m_material; }

        void setMesh( const std::shared_ptr<TMesh>& p_mesh )
        {
            m_mesh = p_mesh;
        }

        void setMaterial( const std::shared_ptr<TMaterial>& p_material )
        {
            m_material = p_material;
        }

    private:
        std::shared_ptr<TMesh>     m_mesh;
        std::shared_ptr<TMaterial> m_material;
    };
} // Tomos
