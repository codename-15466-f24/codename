#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "UIHandler.hpp"

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


	struct TextureItem{
	// handles
		GLuint tex = 0;
		GLuint tristrip = 0; // buffer for tristrip
		GLuint tristrip_for_texture_program = 0; // vao

		GLsizei count = 0; 
		glm::mat4 CLIP_FROM_LOCAL = glm::mat4(1.0f);
		std::string path = "error.png";
		bool loadme = false;
		glm::uvec2 size;
		std::vector<glm::u8vec4> data;
		//x0, x1, y0, y1, z
		std::vector<float> bounds = {-1.0f, 1.0f, -1.0f, -0.33f, 0.0f};
	} tex_box_text, tex_textbg, tex_cs;


	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, downArrow, upArrow, mouse;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	

	
	//Color constants because why not
	glm::u8vec4 white = glm::u8vec4(255,255,255,1);
	glm::u8vec4 red = glm::u8vec4(255,0,0,1);
	glm::u8vec4 green = glm::u8vec4(0,255,0,1);
	glm::u8vec4 blue = glm::u8vec4(0,0,255,1);

	//These are here 
	//void render_text(std::string line_in);
	//void update_texture(TextureItem *tex_in, std::string path);

	std::vector<TextureItem> allTextures;
	
	
	std::vector<TexStruct *> textures;

	bool hasRescaled = false;

	// use these to initialize textures
	// note: the buttons (for toggling the menu on/off) 
	// have to be first in the vector for the visibility to work properly

	// right is true, left is false
	std::vector<PanePosition> alignments = {LeftPane, RightPane, 
											LeftPane, RightPane};
	std::vector<std::string> paths = {"special_request_collapsed.png", "cipher_panel.png",
									"special_request.png", "cipher_panel_full.png"};
									
	std::vector<std::function<void(std::vector<TexStruct *>)>> callbacks;

	//stuff in the scene
	Scene::Transform *swap_creature = nullptr;
	float x_by_counter = 2.9f;
	float creature_speed = 3.0f;
	std::vector<Scene::Transform *> creature_xforms = {swap_creature};

	// coloring
	std::vector<float> colorscheme = {
		0., 0., 0.,
	    9. / 255., 4. / 255., 70. / 255.,
        56. / 255., 79. / 255., 113. / 255.,
        102. / 255., 153. / 255., 155. / 255.,
        167. / 255., 194. / 255., 150. / 255.,
        231. / 255., 235. / 255., 144. / 255.,
    	};
	// for (uint8_t i = 0; i < colorscheme.size(); i++) colorscheme[i] /= 255.;

	//camera:
	Scene::Camera *camera = nullptr;

	// toss in the nightmare loop for now..
	void render_text(TextureItem *tex_in, std::string line_in, glm::u8vec4 color, char cipher, int font_size);

	// game character
	struct GameCharacter {
		std::string id;
		std::string name;
		std::string species; // can change this type later
		// any other data here. maybe assets?
		uint8_t asset_idx;
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
		CHOICE_IMAGE,
		INPUT
	};
	struct DisplayState {
		std::string file = "first_interaction.txt"; // whatever we initialize this to is the start of the script
		std::vector<std::string> current_lines;
		uint32_t line_number = 0;
		// Note: line number, jump, etc. are according to the script, so 1-indexed.
		// Unfortunately, current_lines itself is 0-indexed.

		enum Status status = CHANGING;
		std::string bottom_text = "";
		char cipher = 'd';
		std::vector<DisplayCharacter> chars;
		std::vector<DisplayImage> images; // extra images to be displayed on screen

		std::vector<uint32_t> jumps; // only 1 option if the status isn't a choice
		std::vector<std::string> jump_names; // for choices
		
		std::vector<std::pair<std::string, uint32_t>> history; // for backing up
		uint32_t current_choice = 0;
	} display_state;

	std::string player_id = "player";
	std::string cursor_str = "|";

	void refresh_display();

	void advance_one_line(uint32_t jump_choice);
	void update_one_line(uint32_t location);

	void apply_command(std::string line);

	void retreat_state();
	void advance_state(uint32_t jump_choice);
	void draw_state_text();

	void check_jump(std::string input, std::string correct, uint32_t correctJump, uint32_t incorrectJump);
	void initializeCallbacks();

};
