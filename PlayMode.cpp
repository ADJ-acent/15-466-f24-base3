#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <cmath>

GLuint main_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > main_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("main.pnct"));
	main_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > main_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("main.scene"), [](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = main_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = main_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});

std::array<Load< Sound::Sample >, 3> spawn_sounds = {
	Load< Sound::Sample >(LoadTagDefault, []() -> Sound::Sample const * {
		return new Sound::Sample(data_path("audio/left.opus"));
	}), 
	Load< Sound::Sample >(LoadTagDefault, []() -> Sound::Sample const * {
		return new Sound::Sample(data_path("audio/mid.opus"));
	}),
	Load< Sound::Sample >(LoadTagDefault, []() -> Sound::Sample const * {
		return new Sound::Sample(data_path("audio/right.opus"));
	})
};
std::array<Load< Sound::Sample >, 3> spawn_begin_sounds = {
	Load< Sound::Sample > (LoadTagDefault, []() -> Sound::Sample const * {
		return new Sound::Sample(data_path("audio/left_begin.opus"));
	}),
	Load< Sound::Sample >(LoadTagDefault, []() -> Sound::Sample const * {
		return new Sound::Sample(data_path("audio/mid_begin.opus"));
	}),
	Load< Sound::Sample >(LoadTagDefault, []() -> Sound::Sample const * {
		return new Sound::Sample(data_path("audio/right_begin.opus"));
	})
};

Load< Sound::Sample > caught_carrot_sound(LoadTagDefault, []() -> Sound::Sample const * {
		return new Sound::Sample(data_path("audio/caught.opus"));
});

Load< Sound::Sample > take_damage_sound(LoadTagDefault, []() -> Sound::Sample const * {
		return new Sound::Sample(data_path("audio/hurt.opus"));
});

Load< Sound::Sample > lose_sound(LoadTagDefault, []() -> Sound::Sample const * {
		return new Sound::Sample(data_path("audio/defeat.opus"));
});

Load< Sound::Sample > morning_dew_bgm(LoadTagDefault, []() -> Sound::Sample const * {
		return new Sound::Sample(data_path("audio/morning_dew.opus"));
});

// set up pseudo random number generator:
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<int>index_dist(0, 2);

PlayMode::PlayMode() : 
		scene(*main_scene),
		carrot_paths{
			CarrotPath(glm::vec3(), glm::vec3()),  // left CarrotPath
			CarrotPath(glm::vec3(), glm::vec3()),  // middle CarrotPath
			CarrotPath(glm::vec3(), glm::vec3())   // right CarrotPath
		} 
{

	// cache objects handles
	for (auto &transform : scene.transforms) {
		// get pointer to carrot
		if (transform.name.substr(0, 7) == "CarrotP") {
			idle_carrots.push_back(Carrot{&transform});
		}
		// get pointer to hamster
		else if (transform.name == "Hamster") {
			hamster = &transform;
		}
		// get pointer to spawn points and lose points
		else if (transform.name == "SpawnLeft") {
			carrot_paths[0].start_pos = transform.position;
		}
		else if (transform.name == "LoseLeft") {
			carrot_paths[0].end_pos = transform.position;
		}
		else if (transform.name == "SpawnMiddle") {
			carrot_paths[1].start_pos = transform.position;
		}
		else if (transform.name == "LoseMiddle") {
			carrot_paths[1].end_pos = transform.position;
		}
		else if (transform.name == "SpawnRight") {
			carrot_paths[2].start_pos = transform.position;
		}
		else if (transform.name == "LoseRight") {
			carrot_paths[2].end_pos = transform.position;
		}
		else if (transform.name == "CarrotCluster0") {
			carrot_pile_transforms[0] = &transform;
			transform.enabled = true; // disable rendering at the start
		}
		else if (transform.name == "CarrotCluster1") {
			carrot_pile_transforms[1] = &transform;
			transform.enabled = false;
		}
		else if (transform.name == "CarrotCluster2") {
			carrot_pile_transforms[2] = &transform;
			transform.enabled = false;
		}
		else if (transform.name == "CarrotCluster3") {
			carrot_pile_transforms[3] = &transform;
			transform.enabled = false;
		}
		else if (transform.name == "MenuCamLocation") {
			menu_pos = transform.position;
			menu_quat = transform.rotation;
			transform.enabled = false;
		}
	}

	{// check that all needed transforms are imported correctly
		if (hamster == nullptr) throw std::runtime_error("Hamster not found.");
		if (idle_carrots.size() != 24) throw std::runtime_error("Base carrot not found.");
		std::string path_names[3] = {"left", "middle", "right"};
		for (uint8_t i = 0; i < carrot_paths.size(); ++i) {
			if (carrot_paths[i].start_pos == glm::vec3(0) || carrot_paths[i].end_pos == glm::vec3(0)) {
				throw std::runtime_error("The " + path_names[i] + " path coordinates not complete.");
			}
			// set the end position z to same as start z, z dragged down in the scene to prevent rendering:
			carrot_paths[i].end_pos.z = carrot_paths[i].start_pos.z;
		}
		for (uint8_t i = 0; i < carrot_pile_transforms.size(); ++i) {
			if (carrot_pile_transforms[i] == nullptr) {
				throw std::runtime_error("Carrot Pile " + std::to_string(i) + " not found");
			}
		}
	}

	//cache hamster location
	hamster_default_pos = hamster->position;
	//cache carrot location
	carrot_default_pos = idle_carrots.front().transform->position;

	{//precompute hamster rotations
		hamster_rotations[0] = glm::angleAxis(
			glm::radians(-45.0f),
			glm::vec3(0.0f, 0.0f, 1.0f)
		);
		hamster_rotations[1] = hamster->rotation;
		hamster_rotations[2] = glm::angleAxis(
			glm::radians(45.0f),
			glm::vec3(0.0f, 0.0f, 1.0f)
		);
	}

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	in_game_pos = camera->transform->position;
	in_game_quat = camera->transform->rotation;
	camera->transform->position = menu_pos;
	camera->transform->rotation = menu_quat;

	// set sound locations
	for (uint8_t i = 0; i < carrot_paths.size(); ++i) {
		sound_locations[i] = carrot_paths[i].end_pos;
		sound_locations[i].z = camera->transform->position.z;
	}

	{ //update listener to camera position:
		//moved to initialization since camera is static
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

	Sound::loop(*morning_dew_bgm,0.1f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_r) {
			restart.downs +=1;
			restart.pressed = true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			start.downs +=1;
			start.pressed = true;
		} 
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_r) {
			restart.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			start.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	
	if (menu && start.pressed) {
		menu = false;
		camera->transform->position = in_game_pos;
		camera->transform->rotation = in_game_quat;
		for (uint8_t i = 0; i < carrot_pile_transforms.size(); ++i) {
			carrot_pile_transforms[i]->enabled = false;
		}
	}

	if (menu) {
		menu_timer += elapsed / 5.0f;
		menu_timer -= std::floor(menu_timer);
		return;
	}

	if (restart.pressed) {
		game_end = false;
		since_carrot_spawned = 0.0f;
		carrot_spawn_timer = 2.0f;
		carrot_speed = .5f;
		since_caught = 0.0f;

		//reset tutorial
		tutorial = true;
		tutorial_carrot_count = 0;
		
		score = 0;
		health = 3;

		hamster->position = hamster_default_pos;
		hamster->rotation = hamster_rotations[1];

		for (auto carrot_it = in_action_carrots.begin(); carrot_it != in_action_carrots.end(); ) {
			carrot_it->transform->position = carrot_default_pos;
			idle_carrots.push_back(*carrot_it);
			carrot_it = in_action_carrots.erase(carrot_it);
			continue;
		}

		for (uint8_t i = 0; i < carrot_pile_transforms.size(); ++i) {
			carrot_pile_transforms[i]->enabled = false;
		}
	}

	if (game_end) return;

	{// update caught timer
		if (since_caught != 0.0f) {
			since_caught += elapsed;
			if (since_caught > max_caught_time) {
				since_caught = 0.0f;
				caught_carrot->transform->position = carrot_default_pos;
				idle_carrots.push_back(*caught_carrot);
				in_action_carrots.erase(caught_carrot);
			}
			else { // carrot arrest animation
				caught_carrot->transform->scale = glm::vec3((.5f - since_caught) * 2.0f);
				caught_carrot->transform->position.z += elapsed * 10.0f;
			}
		}
	}

	{// hamster eats carrots
		uint8_t sum = uint8_t(left.pressed) +  uint8_t(right.pressed) + uint8_t(down.pressed);
		if (sum == 1) { // only one lane at a time
			uint8_t hamster_path_index = uint8_t(left.pressed) * 0 +  uint8_t(right.pressed) * 2 + uint8_t(down.pressed) * 1;
			for (auto carrot_it = in_action_carrots.begin(); carrot_it != in_action_carrots.end(); ++ carrot_it) {
				if (carrot_it->on_screen && carrot_it->path_index == hamster_path_index && !carrot_it->caught) {
					if (since_caught != 0.0f) { // get rid of the previous caught carrot
						since_caught = 0.0f;
						caught_carrot->transform->position = carrot_default_pos;
						idle_carrots.push_back(*caught_carrot);
						in_action_carrots.erase(caught_carrot);
					}

					caught_carrot = carrot_it;
					carrot_it->caught = true;
					hamster->position = carrot_paths[carrot_it->path_index].start_pos + (carrot_paths[carrot_it->path_index].end_pos - carrot_paths[carrot_it->path_index].start_pos) * (carrot_it->t + .1f);
					hamster->rotation = hamster_rotations[hamster_path_index];
					since_caught = 0.01f;
					score++;
					
					{// increase height of the carrot pile when score is high enough
						if (score > 150) {
							carrot_pile_transforms[3]->enabled = true;
						} else if (score > 100) {
							carrot_pile_transforms[2]->enabled = true;
						} else if (score > 50) {
							carrot_pile_transforms[1]->enabled = true;
						} else if (score > 25) {
							carrot_pile_transforms[0]->enabled = true;
						} 
					}
					Sound::play_3D(*caught_carrot_sound, 1.0f, carrot_it->transform->position, 200.0f);
					break;
				}
			}
			if (since_caught == 0.0f) { //did not have anything to catch
				hamster->position = carrot_paths[hamster_path_index].end_pos;
				hamster->rotation = hamster_rotations[hamster_path_index];
			}

		}
		else if (since_caught == 0.0f) {
			hamster->position = hamster_default_pos;
			hamster->rotation = hamster_rotations[1];
		}
		
	}

	// move carrots along
	for (auto carrot_it = in_action_carrots.begin(); carrot_it != in_action_carrots.end(); ) {
		if (carrot_it->caught) {
			carrot_it++;
			continue;
		}
		carrot_speed = std::min(carrot_speed + 0.05f, 1.5f);
		carrot_it->t += elapsed*carrot_speed;
		if (carrot_it->t >= 1.0f) {
			// player loses health
			health--;
			if (health > 0)
				Sound::play_3D(*take_damage_sound, 1.0f, carrot_it->transform->position, 200.0f);
			else
				Sound::play(*lose_sound, 1.0f);
			carrot_it->transform->position = carrot_default_pos;
			idle_carrots.push_back(*carrot_it);
			carrot_it = in_action_carrots.erase(carrot_it);
			continue;
		}
		else if (!carrot_it->on_screen && ((carrot_it->path_index == 1 && carrot_it->t >= .03f) || (carrot_it->path_index != 1 && carrot_it->t >= .08f))) {
			carrot_it->on_screen = true;
			float audio_level = carrot_it->path_index == 1 ? 0.7f : 1.0f;
			if (tutorial)
				Sound::play_3D(*(spawn_begin_sounds[carrot_it->path_index]), 1.0f, sound_locations[carrot_it->path_index], 2000.0f);
			else
				Sound::play_3D(*(spawn_sounds[carrot_it->path_index]), audio_level, sound_locations[carrot_it->path_index], 2000.0f);
		}
		// move along path
		carrot_it->transform->position = carrot_paths[carrot_it->path_index].start_pos + (carrot_paths[carrot_it->path_index].end_pos - carrot_paths[carrot_it->path_index].start_pos) * carrot_it->t;
		// movement animation
		carrot_it->transform->scale.z = std::sin(carrot_it->t*20.0f)*0.25f + 1.0f;
		float xy_scale = std::sin(carrot_it->t*20.0f)*-0.25f + 1.0f;
		carrot_it->transform->scale.x = xy_scale;
		carrot_it->transform->scale.x = xy_scale;
		carrot_it++;
    }
	if (!tutorial) // ramp up spawn speed
		carrot_spawn_timer = std::max(carrot_spawn_timer-elapsed*0.05f, 0.5f);


	auto spawn_carrots = [&](uint8_t count){ // 1, 2 or 3 carrots can be spawned together
		for (uint8_t i = 0; i < count; ++i) {
			if (idle_carrots.empty()) return;
			Carrot new_carrot = idle_carrots.front();
			idle_carrots.pop_front();
			//reset carrot
			if (tutorial) {
				new_carrot.path_index = tutorial_carrot_count % 3;
				tutorial_carrot_count++;
				if (tutorial_carrot_count == tutorial_carrot_max)
					tutorial = false;
			}
			else
				new_carrot.path_index = uint8_t(index_dist(gen));
			new_carrot.on_screen = false;
			new_carrot.caught = false;
			new_carrot.t = 0.0f;
			new_carrot.transform->scale = glm::vec3(1.0f);
			in_action_carrots.push_back(new_carrot);
		}
	};

	{//spawn carrots
		since_carrot_spawned += elapsed;
		if (since_carrot_spawned >= carrot_spawn_timer && !idle_carrots.empty()) {
			since_carrot_spawned = std::fmod(since_carrot_spawned, carrot_spawn_timer);
			if (carrot_spawn_timer < .75f) {
				spawn_carrots(uint8_t(index_dist(gen)) + 1);
			}
			else if (carrot_spawn_timer < 1.25f){
				spawn_carrots(uint8_t(std::max(index_dist(gen), 1)));
			}
			else {
				spawn_carrots(1);
			}
		}
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	down.downs = 0;
	restart.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	{//let player know they are dead
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));
		constexpr float H = 0.09f;
		if (game_end) {
			glClearColor(0.6235f, .7569f, .561f, 1.0f); // green like the grass
			float ofs = 6.0f / drawable_size.y;
			lines.draw_text("CARROTS ESCAPED",
				glm::vec3(-float(drawable_size.x)*H / 150.0f, 0.35f, 0.0),
				glm::vec3(H*3, 0.0f, 0.0f), glm::vec3(0.0f, H*3, 0.0f),
				glm::u8vec4(0xec, 0x76, 0x09, 0x00));
			lines.draw_text("CARROTS ESCAPED",
				glm::vec3(-float(drawable_size.x)*H / 150.0f + ofs, ofs +0.35f, 0.0),
				glm::vec3(H*3, 0.0f, 0.0f), glm::vec3(0.0f, H*3, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("Press 'r' to restart",
				glm::vec3(-float(drawable_size.x)*H / 225.0f, -0.5f, 0.0),
				glm::vec3(H*2.0f, 0.0f, 0.0f), glm::vec3(0.0f, H*2.0f, 0.0f),
				glm::u8vec4(0xec, 0x76, 0x09, 0x00));
			lines.draw_text("Press 'r' to restart",
				glm::vec3(-float(drawable_size.x)*H  / 225.0f+ofs, -.5f, 0.0),
				glm::vec3(H*2, 0.0f, 0.0f), glm::vec3(0.0f, H*2.0f, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			lines.draw_text("Score: " + std::to_string(score),
				glm::vec3(-float(drawable_size.x)*H / 350.0f, -.25f, 0.0),
				glm::vec3(H*2, 0.0f, 0.0f), glm::vec3(0.0f, H*2, 0.0f),
				glm::u8vec4(0xec, 0x76, 0x09, 0x00));
			lines.draw_text("Score: " + std::to_string(score),
				glm::vec3(-float(drawable_size.x)*H / 350.0f + ofs, -0.25f + ofs, 0.0),
				glm::vec3(H*2, 0.0f, 0.0f), glm::vec3(0.0f, H*2, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			glClearDepth(1.0f); 
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			GL_ERRORS(); //print any errors produced by this setup code
			return;
		}
	}

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	if (!menu) { //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Use A,S,D to prevent carrots from freeing their comrads",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Use A,S,D to prevent carrots from freeing their comrads",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));

		lines.draw_text("Score: " + std::to_string(score),
			glm::vec3(-aspect + 0.1f * H, 1.0 - H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		lines.draw_text("Score: " + std::to_string(score),
			glm::vec3(-aspect + 0.1f * H + ofs, 1.0 - H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));

		//health
		std::string health_text = "x x x";
		if (health == 3) health_text = "o o o";
		else if (health == 2) health_text = "o o x";
		else if (health == 1) health_text = "o x x";
		else {
			game_end = true;
		}
		lines.draw_text("Cage health: " + health_text,
			glm::vec3(-aspect + 0.1f * H, 1.0 - H*3.0f, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		lines.draw_text("Cage health: " + health_text,
			glm::vec3(-aspect + 0.1f * H + ofs, 1.0 - H*3.0f + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));

	}
	else {
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.20f;
		
		lines.draw_text("     Moth to a Flame",
			glm::vec3(-aspect + 0.1f * H, 1.0f - 7.0f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("     Moth to a Flame ",
			glm::vec3(-aspect + 0.1f * H + ofs, 1.0f - 7.0f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("         Carrot to a Cage",
			glm::vec3(-aspect + 0.1f * H, 1.0f - 8.8f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		lines.draw_text("         Carrot to a Cage",
			glm::vec3(-aspect + 0.1f * H + ofs, 1.0f - 8.8f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		
		uint8_t color = uint8_t(int((std::sin(menu_timer*2.0f*float(M_PI)) + 1.0f) * 127.5f));
		lines.draw_text("             Press space to start...",
			glm::vec3(-aspect + 3.0f * H, 1.0f - 9.5f * H, 0.0),
			glm::vec3(H/2.0f, 0.0f, 0.0f), glm::vec3(0.0f, H/2.0f, 0.0f),
			glm::u8vec4(0xff-color, 0xff-color, 0xff-color, 0x00));
		lines.draw_text("             Press space to start...",
			glm::vec3(-aspect + 3.0f * H + ofs, 1.0f - 9.5f * H + ofs, 0.0),
			glm::vec3(H/2.0f, 0.0f, 0.0f), glm::vec3(0.0f, H/2.0f, 0.0f),
			glm::u8vec4(color, color, color, 0x00));
		
			
		
	}
	GL_ERRORS();
}