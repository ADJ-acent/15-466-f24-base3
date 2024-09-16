#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint main_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > main_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("main.pnct"));
	main_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

auto on_drawable = [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = main_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = main_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
};

Load< Scene > main_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("main.scene"), on_drawable);
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

PlayMode::PlayMode() : 
		scene(*main_scene),
		carrot_paths{
			CarrotPath(glm::vec3(), glm::vec3()),  // left CarrotPath
			CarrotPath(glm::vec3(), glm::vec3()),  // middle CarrotPath
			CarrotPath(glm::vec3(), glm::vec3())   // right CarrotPath
		} 
{
	// reserve enough space for carrots
	in_action_carrots.reserve(max_carrots);
	idle_carrots.reserve(max_carrots);

	for (auto &transform : scene.transforms) {
		// get pointer to carrot
		if (transform.name.substr(0, 7) == "CarrotP") {
			idle_carrots.emplace_back(Carrot{&transform});
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
	}

	// check that all needed transforms are imported correctly
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

	//cache hamster location
	hamster_pos = hamster->position;

	//precompute hamster rotations
	hamster_rotations[2] = glm::angleAxis(
		glm::radians(45.0f),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	hamster_rotations[0] = glm::angleAxis(
		glm::radians(-45.0f),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	hamster->rotation = hamster_rotations[0];

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

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
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	spawn_interval += elapsed;
	if (spawn_interval >= 1.0f) {
		spawn_interval -= 1.0f;
		std::cout<<"Camera Pos:" <<camera->transform->position.x << ", " << camera->transform->position.y << ", "<<camera->transform->position.z <<std::endl;
		std::cout<<"sound Pos:" <<sound_locations[index].x << ", " << sound_locations[index].y << ", "<<sound_locations[index].z <<std::endl;
		Sound::play_3D(*(spawn_sounds[index]), 1.0f, sound_locations[index], 2000.0f);
		index = (index + 1) %3;
	}

	// //move camera:
	// {

	// 	//combine inputs into a move:
	// 	constexpr float PlayerSpeed = 30.0f;
	// 	glm::vec2 move = glm::vec2(0.0f);
	// 	if (left.pressed && !right.pressed) move.x =-1.0f;
	// 	if (!left.pressed && right.pressed) move.x = 1.0f;
	// 	if (down.pressed && !up.pressed) move.y =-1.0f;
	// 	if (!down.pressed && up.pressed) move.y = 1.0f;

	// 	//make it so that moving diagonally doesn't go faster:
	// 	if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

	// 	glm::mat4x3 frame = camera->transform->make_local_to_parent();
	// 	glm::vec3 frame_right = frame[0];
	// 	//glm::vec3 up = frame[1];
	// 	glm::vec3 frame_forward = -frame[2];

	// 	camera->transform->position += move.x * frame_right + move.y * frame_forward;
	// }

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
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

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera; WASD moves; escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}