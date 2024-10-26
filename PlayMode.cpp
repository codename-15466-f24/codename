#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <fstream>
#include <sstream>

GLuint hexapod_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > hexapod_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("hexapod.pnct"));
	hexapod_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > hexapod_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("hexapod.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = hexapod_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = hexapod_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > dusty_floor_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("dusty-floor.opus"));
});

PlayMode::PlayMode() : scene(*hexapod_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Hip.FL") hip = &transform;
		else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
		else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	}
	if (hip == nullptr) throw std::runtime_error("Hip not found.");
	if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	// (note: position will be over-ridden in update())
	leg_tip_loop = Sound::loop_3D(*dusty_floor_sample, 1.0f, get_leg_tip_position(), 10.0f);

	// read from the file
	display_state.current_lines = lines_from_file(display_state.file);
	display_state.jumps.push_back(0);

	update_state();
}

/* This function should refresh the display according to whatever is in the state.
 * Might be necessary at the very start.
 * Could also be useful if something goes wrong.
 */
void PlayMode::refresh_display() {
	// draw bottom_text
	// draw the characters and images displayed
	// these are all comments because I'm not sure what exactly this should look like yet
}

// Advance the script by one line.
void PlayMode::update_one_line(uint32_t jump_index) {
	std::string line = display_state.current_lines[display_state.jumps[jump_index]];
	std::vector<std::string> parsed = parse_script_line(line, " ");

	display_state.line_number = atoi(parsed[0].c_str()) - 1; // indexing problem fix
	std::string keyword = parsed[1];
	if (keyword == "Character") {
		if (characters.find(parsed[2]) == characters.end()) {
			GameCharacter g;
			g.id = parsed[2];
			g.name = parsed[3];
			g.species = parsed[4];
			characters[parsed[2]] = g;
		}
		else {
			std::cerr << parsed[0] + ": Found a character with this ID already. No action taken" << std::endl;
		}
		display_state.status = CHANGING;
	}
	else if (keyword == "Display") {
		if (characters.find(parsed[2]) != characters.end()) {
			// loop through existing characters on display, which should be fine since there aren't many
			for (auto it = display_state.chars.begin(); it != display_state.chars.end(); ++it) {
				if (it->ref->id == parsed[2]) display_state.chars.erase(it);
			}
			DisplayCharacter dc;
			dc.ref = &characters[parsed[2]];
			dc.pos = MIDDLE; // TODO: change depending on code
			display_state.chars.push_back(dc);
		}
		else {
			std::cerr << parsed[0] + ": There was no character with ID " + parsed[2] << std::endl;
		}
		display_state.status = CHANGING;
	}
	else if (keyword == "Remove") {
		if (characters.find(parsed[2]) != characters.end()) {
			// loop through existing characters on display, which should be fine since there aren't many
			for (auto it = display_state.chars.begin(); it != display_state.chars.end(); ++it) {
				if (it->ref->id == parsed[2]) display_state.chars.erase(it);
			}
		}
		display_state.status = CHANGING;
	}
	else if (keyword == "Clear") {
		display_state.chars.clear();
		display_state.images.clear();
		display_state.status = CHANGING;
	}
	else if (keyword == "Speech") {
		if (parsed[2] == player_id) display_state.bottom_text = parsed[3];
		else {
			// TODO: speech bubble stuff; not implemented yet
			display_state.bottom_text = parsed[3];
		}
		display_state.status = TEXT;
	}
	else if (keyword == "Text") {
		display_state.bottom_text = parsed[2];
		display_state.status = TEXT;
	}
	else if (keyword == "Input") {
		display_state.bottom_text = parsed[2];
		// something similar but with text input like we discussed
		display_state.status = TEXT;
	}
	else if (keyword == "Image") {
		// TODO
		display_state.status = IMAGE;
	}
	else if (keyword == "Change_File") {
		display_state.file = parsed[2];
		display_state.current_lines = lines_from_file(display_state.file);
		display_state.bottom_text = "";
		display_state.line_number = 0;
		display_state.jumps.clear();
		display_state.jumps.push_back(0);
		display_state.status = CHANGING;
	}

	// jump-modifying keywords
	if (keyword == "Choice_Text") {
		uint32_t count = atoi(parsed[2].c_str());
		count = 0;
		// TODO

		display_state.status = CHOICE_TEXT;
	}
	else if (keyword == "Choice_Image") {
		// TODO
		display_state.status = CHOICE_IMAGE;
	}
	else if (keyword == "Jump") {
		display_state.jumps = {(uint32_t)atoi(parsed[2].c_str() - 1)};
		display_state.status = CHANGING;
	}
	else display_state.jumps = {display_state.line_number + 1}; // ensure we go to the next line
}

// This is the main implementation. This should advance the game's script until the player needs to advance the display again.
// In other words, things like character displays should run automatically.
void PlayMode::update_state() {
	update_one_line(0);
	while (display_state.status == CHANGING) update_one_line(0);
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
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		// if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
		// 	SDL_SetRelativeMouseMode(SDL_TRUE);
		// 	return true;
		// }
		update_state();
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//slowly rotates through [0,1):
	wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);

	//move sound to follow leg tip position:
	leg_tip_loop->set_position(get_leg_tip_position(), 1.0f / 60.0f);

	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 30.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}

	{ //update listener to camera position:
		glm::mat4x3 frame = camera->transform->make_local_to_parent();
		glm::vec3 frame_right = frame[0];
		glm::vec3 frame_at = frame[3];
		Sound::listener.set_position_right(frame_at, frame_right, 1.0f / 60.0f);
	}

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
		lines.draw_text(display_state.bottom_text,
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(display_state.bottom_text,
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
	GL_ERRORS();
}

glm::vec3 PlayMode::get_leg_tip_position() {
	//the vertex position here was read from the model in blender:
	return lower_leg->make_local_to_world() * glm::vec4(-1.26137f, -11.861f, 0.0f, 1.0f);
}
