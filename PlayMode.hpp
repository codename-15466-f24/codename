#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "UIHandler.hpp"

#include "string_parsing.hpp"
#include "ToggleCipher.hpp"
#include "ReverseCipher.hpp"
#include "CaesarCipher.hpp"
#include "SubstitutionCipher.hpp"

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


	enum Position {
			LEFT,
			MIDDLE,
			RIGHT
	};

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
		glm::uvec2 margin = glm::uvec2(0, 0);
		int f_size = 72;
		Position align = LEFT;
		std::vector<glm::u8vec4> data;
		//x0, x1, y0, y1, z
		std::vector<float> bounds = {-1.0f, 1.0f, -1.0f, -0.33f, 0.0f};
		bool visible = false;
	} tex_box_text, tex_textbg, tex_cs, tex_minipuzzle, tex_special, tex_rev;

	TextureItem *tex_special_ptr;
	TextureItem *tex_minipuzzle_ptr;
	TextureItem *tex_rev_ptr;
	TextureItem *tex_cs_ptr;

	enum Cipher {
		Reverse,
		Substitution
	};

	//input tracking:
	struct Button {
		uint8_t pressed = 0;
	} downArrow, upArrow, mouse;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	

	
	// //Color constants because why not
	glm::u8vec4 white = glm::u8vec4(255,255,255,1);
	// glm::u8vec4 red = glm::u8vec4(255,0,0,1);
	glm::u8vec4 green = glm::u8vec4(0,255,0,1);
	// glm::u8vec4 blue = glm::u8vec4(0,0,255,1);

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
	std::vector<bool> visibilities = {
									true, false, // inventory
									false, true, // request(collapsed), cipher
									false, false,  // request, cipher(full)
									true,          // bg_customer
									false, false,   // customer: blub
									false, false,  // customer: subeelb
									false, false,  // customer: gremlin
									false, false,  // customer: csm1
									false, false,  // customer: csm2
									false, false,  // customer: sp_shaper
									false, false,  // customer: g1_shaper
									false, false,  // customer: g2_shaper
									false, false,  // customer: g3_shaper
									false, false, false, false, // mini puzzle
									false, false
									};

	std::vector<PanePosition> alignments = {LeftPaneMiddle, LeftPaneMiddle,
											LeftPane, RightPane,
											LeftPane, RightPane,
											TopMiddlePaneBG, 
											TopMiddlePaneHidden, TopMiddlePaneHidden,
											TopMiddlePaneHidden, TopMiddlePaneHidden,
											TopMiddlePaneHidden, TopMiddlePaneHidden,
											TopMiddlePaneHidden, TopMiddlePaneHidden,
											TopMiddlePaneHidden, TopMiddlePaneHidden,
											TopMiddlePaneHidden, TopMiddlePaneHidden,
											TopMiddlePaneHidden, TopMiddlePaneHidden,
											TopMiddlePaneHidden, TopMiddlePaneHidden,
											TopMiddlePaneHidden, TopMiddlePaneHidden,
											MiddlePaneBG, MiddlePane, MiddlePaneSelected, MiddlePane,
											MiddlePaneBig, MiddlePaneChange
											};
	std::vector<std::string> paths = {"inventory_collapsed.png", "inventory.png",
									"special_request_collapsed.png", "cipher_panel.png",
									"special_request.png", "cipher_panel_full.png", 
									"bg_customer.png", 
									"customer_basicbleeb.png", "customer_basicbleeb_selected.png",
									"customer_subeelb.png", "customer_subeelb_selected.png",
									"customer_gremlin.png", "customer_gremlin_selected.png",
									"customer_csm1.png", "customer_csm1_selected.png",
									"customer_csm2.png", "customer_csm2_selected.png",
									"customer_sp_shaper.png", "customer_sp_shaper_selected.png",
									"customer_g1_shaper.png", "customer_g1_shaper_selected.png",
									"customer_g2_shaper.png", "customer_g2_shaper_selected.png",
									"customer_g3_shaper.png", "customer_g3_shaper_selected.png",
									"mini_puzzle_panel.png", "reverse_button.png","reverse_button_selected.png", "submitbutton.png",
									"big_puzzle_panel.png", "changebutton.png"

									
									};
									
	std::vector<std::function<void(std::vector<TexStruct *>,std::string)>> callbacks;

	//stuff in the scene
	Scene::Transform *sp_shaper  = nullptr;
	Scene::Transform *basicbleeb = nullptr;
	Scene::Transform *subeelb    = nullptr;
	Scene::Transform *gremlin    = nullptr;
	Scene::Transform *csm1       = nullptr;
	Scene::Transform *csm2       = nullptr;
	Scene::Transform *g1_shaper  = nullptr;
	Scene::Transform *g2_shaper  = nullptr;
	Scene::Transform *g3_shaper  = nullptr;
	const float x_by_counter = 2.9f;
	// float x_next_in_line = x_by_counter;
	const float x_entering_store = -14.f;
	const float y_exited_store = 14.f;
	const float creature_speed = 5.0f;
	std::vector<Scene::Transform *> creature_xforms;
	
	// coloring
	std::vector<float> colorscheme = {
		0.0f, 0.0f, 0.0f,
	    9.0f / 255.0f, 4.0f / 255.0f, 70.0f / 255.0f,
        56.0f / 255.0f, 79.0f / 255.0f, 113.0f / 255.0f,
        102.0f / 255.0f, 153.0f / 255.0f, 155.0f / 255.0f,
        167.0f / 255.0f, 194.0f / 255.0f, 150.0f / 255.0f,
        231.0f / 255.0f, 235.0f / 255.0f, 144.0f / 255.0f,
    	};

	//camera:
	Scene::Camera *camera = nullptr;

	// toss in the nightmare loop for now..
	void render_text(TextureItem *tex_in, std::string line_in, 
	                 glm::u8vec4 color, char cipher, int font_size);

	// game character
	struct GameCharacter {
		std::string id;
		std::string name;
		ToggleCipher *species; // can change this type later
		uint8_t joining_line = 0; // 0 = false, 1 = true, 2 = waiting to join line until leave animation finishes
		uint8_t leaving_line = 0; // 0 = false, 1 = true, 2 = waiting to leave line until join animation finishes
		int8_t asset_idx = -1;

		bool character_completed = false;

		std::string entrance_file = "";
	};
	// keys are character id's
	std::unordered_map<std::string,GameCharacter> characters;
	std::unordered_map<std::string,std::string> entrance_filenames;

	/// @warning ⚠️ this is gonna be null when no game character is selected, watch out when dereferencing 🙀
	GameCharacter *selected_character = nullptr;

	// uint8_t customers_in_line = 0;
	// glm::vec3 pos_in_line(float t); // parametric function on a silly little rectangle
	void join_line(GameCharacter *g);
	void leave_line(GameCharacter *g);

	void clean_curr();

	struct Image {
		std::string id;
		std::string path;
		// anything else?
	};
	// Jim said something about having a struct for all characters, for example.
	// I'm not sure how to do that properly here so I'm leaving it like this for now.

	
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
		WAIT_FOR_SOLVE,
		INPUT
	};
	struct DisplayState {
		std::string file = "cs_major_introduction.txt"; // whatever we initialize this to is the start of the script
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

		ToggleCipher *puzzle_cipher = new ToggleCipher();
		bool solved_puzzle = false;
		std::string solution_text;
		std::string puzzle_text;

		ToggleCipher *special_cipher = new ToggleCipher();
		std::string special_solution_text = "No special requests right now!";
		std::string special_request_text = "No special requests right now!";
	} display_state;

	std::string player_id = "player";
	std::string cursor_str = "|";

	size_t counter = 0;

	std::string prev_character = "";


	std::string substitution_display = {'.','.','.','.','.','.','.','.','.','.','.',
                                        '.','.','.','.','.','.','.','.','.','.','.',
										'.','.', '.','.', '\0'};
	std::string *substitution_display_ptr = nullptr;
	// sound
	std::vector<std::shared_ptr< Sound::PlayingSample >> curr_sound;
	std::deque<std::shared_ptr< Sound::PlayingSample >> next_sound;

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
