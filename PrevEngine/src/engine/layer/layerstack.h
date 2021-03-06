#pragma once

#include "layer.h"

namespace prev {

	class LayerStack {
	public:
		LayerStack();
		~LayerStack();

		void PushLayer(Layer * layer);
		void PushOverlay(Layer * overlay);
		void PopLayer(Layer * layer);
		void PopOverlay(Layer * layer);

	public:
		void OnUpdate();
		void OnImGuiUpdate();
		void OnEvent(Event & e);

		Layer * GetLayer(const std::string & layerName);
	private:
		std::vector<Layer *> m_Layers;
		std::vector<Layer *> m_Overlays;
	};

}