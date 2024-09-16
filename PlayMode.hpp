#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <deque>
#include <list>
#include <array>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//start and end points for carrots
	struct CarrotPath {
		glm::vec3 start_pos;
		glm::vec3 end_pos;

		CarrotPath(glm::vec3 start, glm::vec3 end) : start_pos(start), end_pos(end) {};
	};

	// 0 - left, 1 - middle, 2 - right
	std::array<PlayMode::CarrotPath, 3> carrot_paths;

	//carrots
	float carrot_speed = 1.0f;
	const uint8_t max_carrots = 24;
	struct Carrot {
		Scene::Transform* transform;
		float t = 0.0f;
		uint8_t path_index = 0;
		bool on_screen = false;
		bool caught = false;
	};
	std::list<Carrot> in_action_carrots;
	std::deque<Carrot> idle_carrots;
	float since_carrot_spawned = 0.0f;
	float carrot_spawn_timer = 2.0f;
	glm::vec3 carrot_default_pos;

	//hamster
	Scene::Transform* hamster = nullptr;
	std::array<glm::quat, 3> hamster_rotations;
	glm::vec3 hamster_default_pos;
	std::list<PlayMode::Carrot>::iterator caught_carrot;
	float since_caught = 0.0f;
	const float max_caught_time = .5f;

	float since_last_button_press = 0.0f;
	const float button_press_cooldown = .05f;

	//sound locations
	std::array<glm::vec3, 3> sound_locations;
	
	//camera:
	Scene::Camera *camera = nullptr;

	bool tutorial = false;
	uint8_t tutorial_carrot_count = 0;
	const uint8_t tutorial_carrot_max = 10;

};
