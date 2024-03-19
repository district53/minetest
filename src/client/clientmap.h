/*
Minetest
Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#include "irrlichttypes_extrabloated.h"
#include "map.h"
#include "camera.h"
#include "tile.h"
#include <set>
#include <map>
#include "CNullDriver.h"
#include "memoryManager.h"

struct MapDrawControl
{
	// Wanted drawing range
	float wanted_range = 0.0f;
	// Overrides limits by drawing everything
	bool range_all = false;
	// Allow rendering out of bounds
	bool allow_noclip = false;
	// show a wire frame for debugging
	bool show_wireframe = false;
};

namespace irr
{
	namespace scene
	{
		class SMeshBuffer32 : public SMeshBuffer
		{
		public:
			//! Get type of index data which is stored in this meshbuffer.
			/** \return Index type of this buffer. */
			video::E_INDEX_TYPE getIndexType() const override
			{
				return video::EIT_32BIT;
			}

			//! Get pointer to indices
			/** \return Pointer to indices. */
			const u16* getIndices() const override
			{
				return nullptr;
			}

			//! Get pointer to indices
			/** \return Pointer to indices. */
			u16* getIndices() override
			{
				return nullptr;
			}


			//! Get number of indices
			/** \return Number of indices. */
			u32 getIndexCount() const override
			{
				return indexCount;
			}

			u32 getVertexCount() const override
			{
				return vertexCount;
			}

			virtual u32 getPrimitiveCount() const
			{
				return drawPrimitiveCount;
			}

			//! Indices into the vertices of this buffer.
			//core::array<u32> Indices32;

			u32 drawPrimitiveCount = 0;
			u32 indexCount = 0;
			u32 vertexCount = 0;
		};
	}
}

namespace {
	struct SMeshBufferData {
		MemoryManager::MemoryInfo vertexMemory;
		MemoryManager::MemoryInfo indexMemory;

		scene::IMeshBuffer* meshBuffer;
		video::ITexture* texture = nullptr;
		u8 layer = 0;
	};

	struct MeshBlockBuffer {
		MapBlockMesh* mapBlockMesh = nullptr;
		scene::IMeshBuffer* meshBuffer = nullptr;
	};

	struct TextureBufListMaps
	{
		struct TextureHash
		{
			size_t operator()(const video::ITexture* t) const noexcept
			{
				// Only hash first texture. Simple and fast.
				//return std::hash<video::ITexture*>{}(m.TextureLayers[0].Texture);
				return (size_t)t;
			}
		};

		struct Data
		{
			scene::SMeshBuffer32* buffer;

			MemoryManager vertex_memory;
			MemoryManager index_memory;

			Data() :
				buffer(nullptr),
				vertex_memory(sizeof(video::S3DVertex)),
				index_memory(sizeof(u32)*3) {

			}
		};

		using MaterialBufListMap = std::unordered_map<
			video::ITexture *,
			Data *,
			TextureHash>;

		std::array<MaterialBufListMap, MAX_TILE_LAYERS> maps;

		~TextureBufListMaps() {
			clear();
		}

		void clear()
		{
			for (int layer = 0; layer < MAX_TILE_LAYERS; layer++) {
				auto& map = maps[layer];

				for (auto it : map) {
					it.first->drop();
					auto data = it.second;
					data->buffer->drop();
					delete data;
				}

				map.clear();
			}
		}

		Data* getNoCreate(video::ITexture* texture, u8 layer)
		{
			assert(layer < MAX_TILE_LAYERS);

			// Append to the correct layer
			auto& map = maps[layer];
			if (map.find(texture) == map.end())
				return nullptr;

			return map[texture];
		}

		Data* get(video::ITexture* texture, u8 layer)
		{
			assert(layer < MAX_TILE_LAYERS);

			// Append to the correct layer
			auto& map = maps[layer];

			Data* d = map[texture];
			if (d)
				return d;

			texture->grab();
			scene::SMeshBuffer32* buffer = new scene::SMeshBuffer32();
			buffer->MappingHint_Index = scene::E_HARDWARE_MAPPING::EHM_DYNAMIC;
			buffer->MappingHint_Vertex = scene::E_HARDWARE_MAPPING::EHM_DYNAMIC;
			buffer->grab();
			
			d = new Data();
			d->buffer = buffer;

			map[texture] = d;

			return d;
		}

		void drop(video::ITexture* texture, u8 layer)
		{
			assert(layer < MAX_TILE_LAYERS);

			// Append to the correct layer
			auto& map = maps[layer];
			if (map.find(texture) == map.end())
				return;

			texture->drop();

			Data* d = map[texture];
			d->buffer->drop();
			d->buffer = nullptr;
			delete d;
			map.erase(texture);
		}
	};
}

class Client;
class ITextureSource;
class PartialMeshBuffer;

/*
	ClientMap

	This is the only map class that is able to render itself on screen.
*/

class ClientMap : public Map, public scene::ISceneNode
{
public:
	ClientMap(
			Client *client,
			RenderingEngine *rendering_engine,
			MapDrawControl &control,
			s32 id
	);

	virtual ~ClientMap();

	bool maySaveBlocks() override
	{
		return false;
	}

	void drop() override
	{
		ISceneNode::drop(); // calls destructor
	}

	void updateCamera(v3f pos, v3f dir, f32 fov, v3s16 offset, video::SColor light_color);

	/*
		Forcefully get a sector from somewhere
	*/
	MapSector * emergeSector(v2s16 p) override;

	/*
		ISceneNode methods
	*/

	virtual void OnRegisterSceneNode() override;

	virtual void render() override
	{
		video::IVideoDriver* driver = SceneManager->getVideoDriver();
		driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
		renderMap(driver, SceneManager->getSceneNodeRenderPass());
	}

	virtual const aabb3f &getBoundingBox() const override
	{
		return m_box;
	}

	void getBlocksInViewRange(v3s16 cam_pos_nodes,
		v3s16 *p_blocks_min, v3s16 *p_blocks_max, float range=-1.0f);
	void updateDrawList();
	// @brief Calculate statistics about the map and keep the blocks alive
	void touchMapBlocks();
	void updateDrawListShadow(v3f shadow_light_pos, v3f shadow_light_dir, float radius, float length);
	// Returns true if draw list needs updating before drawing the next frame.
	bool needsUpdateDrawList() { return m_needs_update_drawlist; }
	void renderMap(video::IVideoDriver* driver, s32 pass);

	void renderMapShadows(video::IVideoDriver *driver,
			const video::SMaterial &material, s32 pass, int frame, int total_frames);

	int getBackgroundBrightness(float max_d, u32 daylight_factor,
			int oldvalue, bool *sunlight_seen_result);

	void renderPostFx(CameraMode cam_mode);

	// For debug printing
	void PrintInfo(std::ostream &out) override;

	const MapDrawControl & getControl() const { return m_control; }
	f32 getWantedRange() const { return m_control.wanted_range; }
	f32 getCameraFov() const { return m_camera_fov; }

	void onSettingChanged(const std::string &name);

	bool doesNeedToUpdateCache();
	void updateCacheBuffers(video::IVideoDriver* driver);

protected:
	void reportMetrics(u64 save_time_us, u32 saved_blocks, u32 all_blocks) override;
private:
	bool isMeshOccluded(MapBlock *mesh_block, u16 mesh_size, v3s16 cam_pos_nodes);

	// update the vertex order in transparent mesh buffers
	void updateTransparentMeshBuffers();


	// Orders blocks by distance to the camera
	class MapBlockComparer
	{
	public:
		MapBlockComparer(const v3s16 &camera_block) : m_camera_block(camera_block) {}

		bool operator() (const v3s16 &left, const v3s16 &right) const
		{
			auto distance_left = left.getDistanceFromSQ(m_camera_block);
			auto distance_right = right.getDistanceFromSQ(m_camera_block);
			return distance_left > distance_right || (distance_left == distance_right && left > right);
		}

	private:
		v3s16 m_camera_block;
	};


	// reference to a mesh buffer used when rendering the map.
	struct DrawDescriptor {
		v3s16 m_pos;
		union {
			scene::IMeshBuffer *m_buffer;
			const PartialMeshBuffer *m_partial_buffer;
		};
		bool m_reuse_material:1;
		bool m_use_partial_buffer:1;

		DrawDescriptor(v3s16 pos, scene::IMeshBuffer *buffer, bool reuse_material) :
			m_pos(pos), m_buffer(buffer), m_reuse_material(reuse_material), m_use_partial_buffer(false)
		{}

		DrawDescriptor(v3s16 pos, const PartialMeshBuffer *buffer) :
			m_pos(pos), m_partial_buffer(buffer), m_reuse_material(false), m_use_partial_buffer(true)
		{}

		scene::IMeshBuffer* getBuffer();
		void draw(video::IVideoDriver* driver);
	};

	Client *m_client;
	RenderingEngine *m_rendering_engine;

	aabb3f m_box = aabb3f(-BS * 1000000, -BS * 1000000, -BS * 1000000,
		BS * 1000000, BS * 1000000, BS * 1000000);

	MapDrawControl &m_control;

	v3f m_camera_position = v3f(0,0,0);
	v3f m_camera_direction = v3f(0,0,1);
	f32 m_camera_fov = M_PI;
	v3s16 m_camera_offset;
	video::SColor m_camera_light_color = video::SColor(0xFFFFFFFF);
	bool m_needs_update_transparent_meshes = true;

	std::map<v3s16, MapBlock*, MapBlockComparer> m_drawlist;
	std::vector<MapBlock*> m_keeplist;
	std::map<v3s16, MapBlock*> m_drawlist_shadow;
	bool m_needs_update_drawlist;

	std::set<v2s16> m_last_drawn_sectors;

	bool m_cache_trilinear_filter;
	bool m_cache_bilinear_filter;
	bool m_cache_anistropic_filter;
	u16 m_cache_transparency_sorting_distance;

	bool m_loops_occlusion_culler;
	bool m_enable_raytraced_culling;

	TextureBufListMaps cache_buffers;
	core::array<u32> empty_data;

	std::unordered_map<v3s16, MapBlock*> cache_keep_blocks;
	std::unordered_map<MapBlock*, std::vector<SMeshBufferData>> buffer_data;
	std::unordered_set<MapBlock*> render_uncached[2];
	int last_frameno = -1;
};
