#include <iostream>
#include <format>
#include <string_view>

#include <SDL.h>
#include <SDL_image.h>
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_sdlrenderer.h>

#include "blade.hpp"

#define UPDATE_INTERVAL 15000

#define SET_GET(prop, type, base, off) \
	type prop() const { \
		return m_proc.read<type>(base + off); \
	} \
	void set_##prop(const type _x) { \
		m_proc.write<type>(base + off, _x); \
	}

template<typename ...Args>
void print(const std::string_view &fmt,  Args&&...args)
{
	std::cout << std::vformat(fmt, std::make_format_args(args...));
}

class player
{
public:
	player(blade::process& proc, blade::module& mod)
		: m_proc{ proc }
		, m_mod{ mod }
	{
		update();
	}

public:
	blade::addr_t player_script() const
	{
		return m_player_script;
	}

	void update()
	{
		m_player_script = blade::resolve(m_proc, m_mod + 0x1768380, { 0x88, 0xe70, 0x210, 0xf8, 0x78, 0x38, 0x20 });
		m_shoot_script = m_proc.read<blade::addr_t>(m_player_script + 0x28);
		m_pos_base = blade::resolve(m_proc, m_mod + 0x17c8440, { 0x68, 0x18, 0x140, 0x60, 0x70, 0x20 });
	}

	SET_GET(x, blade::f32, m_pos_base, 0x72c);
	SET_GET(y, blade::f32, m_pos_base, 0x730);

	SET_GET(health, blade::u32, m_player_script, 0x42c);
	SET_GET(max_health, blade::u32, m_player_script, 0x430);

	SET_GET(damage, blade::u32, m_shoot_script, 0x90);

private:
	blade::process &m_proc;
	blade::module& m_mod;
	blade::addr_t m_player_script{};
	blade::addr_t m_shoot_script{};
	blade::addr_t m_pos_base{};
};

class camera
{
public:
	camera(blade::process& proc, blade::module& mod)
		: m_proc{ proc }
		, m_mod{ mod }
	{
		update();
	}

public:
	blade::addr_t camera_controller() const
	{
		return m_camera_controller;
	}

	void update()
	{
		// the offsets are almost the same as the player_script's
		// becaue we're using the player scripts camera controller script reference
		// to get the camera controller object
		m_camera_controller = blade::resolve(m_proc, m_mod + 0x1768380, { 0x88, 0xe70, 0x210, 0xf8, 0x78, 0x38, 0x20, 0x138 });
	}

	SET_GET(min_x, blade::f32, m_camera_controller, 0x24);
	SET_GET(min_y, blade::f32, m_camera_controller, 0x28);
	SET_GET(max_x, blade::f32, m_camera_controller, 0x2c);
	SET_GET(max_y, blade::f32, m_camera_controller, 0x30);

private:
	blade::process& m_proc;
	blade::module& m_mod;

	blade::addr_t m_camera_controller{};
};

class application
{
public:
	~application()
	{
		destroy();
	}

public:
	bool create(const char* title, const int w, const int h)
	{
		m_window_w = w;
		m_window_h = h;

		if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
		{
			print("{}\n", SDL_GetError());
			return false;
		}

		if (IMG_Init(IMG_INIT_PNG) == 0)
		{
			print("{}\n", IMG_GetError());
			return false;
		}

		m_window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_SHOWN);
		if (!m_window)
		{
			print("{}\n", SDL_GetError());
			return false;
		}

		m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
		if (!m_renderer)
		{
			print("{}\n", SDL_GetError());
			return false;
		}

		if (!load_assets())
		{
			return false;
		}

		ImGui::CreateContext();
		m_imgui_io = &ImGui::GetIO();

		ImGui::StyleColorsDark();

		if (!ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer))
		{
			print("ImGui: `ImGui_ImplSDL2_InitForSDLRenderer` failed\n");
			return false;
		}

		if (!ImGui_ImplSDLRenderer_Init(m_renderer))
		{
			print("ImGui: `ImGui_ImplSDLRenderer_Init` failed\n");
			return false;
		}

		return true;
	}

	void screen_to_world(const float x, const float y, float &ox, float &oy)
	{
		ox = (x - 499.0f) / 0.77f;
		oy = -((y - 499.0f) / 0.77f);
	}

	void world_to_screen(const float x, const float y, float& ox, float& oy)
	{
		ox = x * 0.77f + 499.0f;
		oy = -y * 0.77f + 499.0f;
	}

	void run()
	{
		Uint32 ticks_since_last_update = 0;
		auto prev_ticks = SDL_GetTicks();

		bool no_boundaries = false;
		bool invincible = false;

		bool running = true;
		while (running)
		{
			const auto current_ticks = SDL_GetTicks();
			const auto elapsed_ticks = current_ticks - prev_ticks;
			prev_ticks = current_ticks;

			ticks_since_last_update += elapsed_ticks;

			// reset everything if the process is not running anymore
			const bool proc_is_running = blade::is_running(m_proc.id());
			if (!proc_is_running)
			{
				if (m_proc.is_open())
				{
					m_proc.close();
				}

				if (m_mod.is_open())
				{
					m_mod.close();
				}

				delete m_ply;
				m_ply = nullptr;

				delete m_cam;
				m_cam = nullptr;
			}

			// try to find the process and module
			if (!m_proc.is_open())
			{
				m_proc.open(L"Calturin.exe");
			}
			else if (!m_mod.is_open())
			{
				m_mod.open(m_proc, L"UnityPlayer.dll");
			}
			else
			{
				if (!m_ply)
				{
					m_ply = new player{ m_proc, m_mod };
				}

				if (!m_cam)
				{
					m_cam = new camera{ m_proc, m_mod };
				}
			}

			SDL_Event event;
			while (SDL_PollEvent(&event) > 0)
			{
				ImGui_ImplSDL2_ProcessEvent(&event);

				switch (event.type)
				{
				case SDL_QUIT:
					running = false;
					break;

				case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_RIGHT)
					{
						int x, y;
						SDL_GetMouseState(&x, &y);

						float wx, wy;
						screen_to_world(x, y, wx, wy);

						m_ply->set_x(wx);
						m_ply->set_y(wy);
					}
					break;

				default:
					break;
				}
			}

			ImGui_ImplSDLRenderer_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();

			if (ImGui::Begin("Tool"))
			{
				if (ImGui::BeginTabBar("Panes"))
				{
					// STATUS
					//
					if (ImGui::BeginTabItem("Status"))
					{
						ImGui::Text("attached to `%s`: %s", "Calturin.exe", m_proc.is_open() ? "yes" : "no");
						ImGui::Text("attached to `%s`: %s", "UnityPlayer.dll", m_mod.is_open() ? "yes" : "no");
						ImGui::Text("module base address: 0x%llx", m_mod.base_address());

						ImGui::Text("Refreshing object addresses in %.2fs", (UPDATE_INTERVAL - ticks_since_last_update) / 1000.0f);

						if (m_ply)
						{
							ImGui::Text("player script: 0x%llx", m_ply->player_script());
						}

						if (m_cam)
						{
							ImGui::Text("camera controller: 0x%llx", m_cam->camera_controller());
						}

						ImGui::EndTabItem();
					}

					// PLAYER
					//
					if (ImGui::BeginTabItem("Player"))
					{
						if (m_ply)
						{
							if (ticks_since_last_update >= UPDATE_INTERVAL)
							{
								m_ply->update();
							}

							// Position
							float pos[2] = { m_ply->x(), m_ply->y() };
							if (ImGui::InputFloat2("Position", pos, "%.2f"))
							{
								m_ply->set_x(pos[0]);
								m_ply->set_y(pos[1]);
							}

							ImGui::Checkbox("Invincible", &invincible);
							
							int flags = ImGuiInputTextFlags_EnterReturnsTrue;
							if (invincible)
							{
								flags |= ImGuiInputTextFlags_ReadOnly;
							}

							// Health
							blade::i32 health = m_ply->health();
							if (ImGui::InputInt("Health", &health, 1, 100, flags))
							{
								m_ply->set_health(health);
							}

							// Max health
							blade::i32 max_health = m_ply->max_health();
							if (ImGui::InputInt("Max health", &max_health, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue))
							{
								m_ply->set_max_health(max_health);
							}

							// Damage
							blade::i32 damage = m_ply->damage();
							if (ImGui::InputInt("Damage", &damage, 1, 100, ImGuiInputTextFlags_EnterReturnsTrue))
							{
								m_ply->set_damage(damage);
							}
						}
						else
						{
							ImGui::Text("waiting to attach process and module...");
						}
						ImGui::EndTabItem();
					}

					// CAMERA
					//
					if (ImGui::BeginTabItem("Camera"))
					{
						if (m_cam)
						{
							if (ticks_since_last_update >= UPDATE_INTERVAL)
							{
								m_cam->update();
							}

							ImGui::Checkbox("No Boundaries", &no_boundaries);

							float min_pos[2] = { m_cam->min_x(), m_cam->min_y() };
							float max_pos[2] = { m_cam->max_x(), m_cam->max_y() };

							int flags = ImGuiInputTextFlags_None;
							if (no_boundaries)
							{
								flags |= ImGuiInputTextFlags_ReadOnly;
							}

							if (ImGui::InputFloat2("Min position", min_pos, "%.2f", flags))
							{
								m_cam->set_min_x(min_pos[0]);
								m_cam->set_min_y(min_pos[1]);
							}
							if (ImGui::InputFloat2("Max position", max_pos, "%.2f", flags))
							{
								m_cam->set_max_x(max_pos[0]);
								m_cam->set_max_y(max_pos[1]);
							}
						}
						else
						{
							ImGui::Text("waiting to attach process and module...");
						}
						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}
			}
			ImGui::End();

			ImGui::Render();

			SDL_RenderClear(m_renderer);

			SDL_RenderCopy(m_renderer, m_map, nullptr, nullptr);

			if (m_ply)
			{
				float x, y;
				world_to_screen(m_ply->x(), m_ply->y(), x, y);
				const SDL_Rect r{
					(int)x - 2, (int)y - 2,
					4, 4
				};

				SDL_SetRenderDrawColor(m_renderer, 255, 255, 255, 255);
				SDL_RenderFillRect(m_renderer, &r);
				SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
			}

			ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
			
			SDL_RenderPresent(m_renderer);

			if (ticks_since_last_update >= UPDATE_INTERVAL)
			{
				ticks_since_last_update = 0;
			}

			// Functionalities
			if (m_ply && invincible)
			{
				m_ply->set_health(10'000);
			}

			if (m_cam && no_boundaries)
			{
				m_cam->set_min_x(-1000.0f);
				m_cam->set_min_y(-1000.0f);
				m_cam->set_max_x(1000.0f);
				m_cam->set_max_y(1000.0f);
			}
		}
	}

	void destroy()
	{
		delete m_ply;

		ImGui_ImplSDLRenderer_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		SDL_DestroyRenderer(m_renderer);
		SDL_DestroyWindow(m_window);
		SDL_Quit();

		m_window = nullptr;
		m_renderer = nullptr;
	}

private:
	bool load_assets()
	{
		m_map = IMG_LoadTexture(m_renderer, "data/map_image.png");
		if (!m_map)
		{
			print("{}\n", IMG_GetError());
			return false;
		}

		return true;
	}

private:
	SDL_Window* m_window{};
	SDL_Renderer* m_renderer{};

	int m_window_w{};
	int m_window_h{};

	ImGuiIO *m_imgui_io{};

	blade::process m_proc;
	blade::module m_mod;

	player *m_ply{};
	camera *m_cam{};

	SDL_Texture* m_map{};
};

int main(int argc, char **argv)
{
	IMGUI_CHECKVERSION();

	application app;
	if (app.create("CalToolrin", 600, 600))
	{
		app.run();
	}


	return 0;
}