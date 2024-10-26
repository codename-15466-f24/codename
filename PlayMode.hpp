#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"

#include "string_parsing.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <unordered_map>
#include <map>

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
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//hexapod leg to wobble:
	Scene::Transform *hip = nullptr;
	Scene::Transform *upper_leg = nullptr;
	Scene::Transform *lower_leg = nullptr;
	glm::quat hip_base_rotation;
	glm::quat upper_leg_base_rotation;
	glm::quat lower_leg_base_rotation;
	float wobble = 0.0f;

	glm::vec3 get_leg_tip_position();

	//music coming from the tip of the leg (as a demonstration):
	std::shared_ptr< Sound::PlayingSample > leg_tip_loop;
	
	//camera:
	Scene::Camera *camera = nullptr;

	// inspired by my game4 code ~Yoseph

	// game character
	struct GameCharacter {
		std::string id;
		std::string name;
		std::string species; // can change this type later
		// any other data here. maybe assets?
	};
	std::unordered_map<std::string, GameCharacter> characters;

	struct Image {
		std::string id;
		std::string path;
		// anything else?
	};
	// Jim said something about having a struct for all characters, for example.
	// I'm not sure how to do that properly here so I'm leaving it like this for now.

	enum Position {
			LEFT,
			MIDDLE,
			RIGHT
	};
	
	struct DisplayCharacter {
		GameCharacter* ref;
		enum Position pos;
	};
	struct DisplayImage {
		Image* ref;
		enum Position pos;
	};

	enum Status {
		CHANGING,
		TEXT,
		CHOICE_TEXT,
		IMAGE,
		CHOICE_IMAGE
	};
	struct DisplayState {
		std::string file = "test.txt"; // whatever we initialize this to is the start of the script
		std::vector<std::string> current_lines;
		uint32_t line_number = 0; // note: this is 0-indexed while the script is 1-indexed currently...

		enum Status status = CHANGING;
		std::string bottom_text = "";
		std::vector<DisplayCharacter> chars;
		std::vector<DisplayImage> images; // extra images to be displayed on screen
		std::vector<uint32_t> jumps; // only 1 option if the status isn't a choice
	} display_state;

	std::string player_id = "player";

	void refresh_display();
	void update_one_line(uint32_t jump_index);
	void update_state();

};
