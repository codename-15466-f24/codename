#include "Mode.hpp"

#include "Scene.hpp"
#include "Sound.hpp"
#include "UIHandler.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

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
	} tex_example, tex_bg, tex_textbg;


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
	glm::u8vec4 blue = glm::u8vec4(0,255,0,1);
	glm::u8vec4 green = glm::u8vec4(0,0,255,1);

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
											LeftPane, LeftPane, LeftPane, LeftPane,
											RightPane};
	std::vector<std::string> paths = {"pressI.png", "pressC.png", 
									"inventory1.png", "inventory1.png", "inventory1.png", "inventory1.png",
									"cheatsheet_placeholder.png"};

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

};
