#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint game_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > game_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("shooting_gallery_meshes.pnct"));
	game_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > game_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("shooting_gallery.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = game_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = game_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

PlayMode::PlayMode() : scene(*game_scene) {

	for (auto &transform : scene.transforms){
		// Get pointers to targets
		if (transform.name.find("Target") != std::string::npos){
			Target found_target = Target(&transform);
			// found_target.collider.transform->position += glm::vec3(0.0f, 0.0f, 5.0f); // shif
			targets.emplace_back(Target(&transform));
		}

		// Get pointer to cannon
		if (transform.name.find("Cannon") != std::string::npos){
			cannon = &transform;
		}
	}

	health_points = 3;
	targets_left = targets.size();
	
	cannon_base_rotation = cannon->rotation;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	
	camera->transform->position -= glm::vec3(-12.0f, 0.0f, 0.0f);

	player_collider = SphereCollider(camera->transform, 2.0f);

	current_game_state = GameState::Play;
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		} else if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_RETURN){
			enter.downs += 1;
			enter.pressed = true;
			if (current_game_state != GameState::Play) return true;
			
			glm::vec3 spawn_position = camera->transform->make_world_from_local() * glm::vec4(0.0f, 0.0f, -5.0f, 1.0f);
			glm::mat4x3 frame = camera->transform->make_parent_from_local();
			glm::vec3 direction = -frame[2]; // get camera's forward

			spawn_projectile(spawn_position, direction, player_projectile_speed);
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_RETURN){
			enter.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == false) {
			SDL_SetWindowRelativeMouseMode(Mode::window, true);
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == true) {
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

	// move projectile
	{
		if (current_projectile.active)
		{
			current_projectile.velocity.z += gravity * elapsed;
			current_projectile.transform->position += current_projectile.velocity * elapsed;

		}
	}

	// check for collision (sphere vs sphere)
	// based on https://www.swiftless.com/tutorials/opengl/collision.html
	{
		if (current_projectile.active){
			// check target collision
			for (Target& target : targets){
				
				if (!target.active) continue;

				// calculate distance between two points
				// sphere collider for target needs to be shifted up for proper collision
				float collider_offset = 5.0f;
				float distance = std::sqrt(
					((current_projectile.transform->position.x - target.transform->position.x) *
					(current_projectile.transform->position.x - target.transform->position.x)) +

					((current_projectile.transform->position.y - target.transform->position.y) *
					(current_projectile.transform->position.y - target.transform->position.y)) +

					((current_projectile.transform->position.z - (target.transform->position.z + collider_offset)) * 
					(current_projectile.transform->position.z - (target.transform->position.z + collider_offset)))
				);

				if (distance <= current_projectile.collider.radius + target.collider.radius){
					handle_target_collision(target);
				}

			
			}

			// check player collision
			float distance = std::sqrt(
					((current_projectile.transform->position.x - player_collider.transform->position.x) *
					(current_projectile.transform->position.x - player_collider.transform->position.x)) +

					((current_projectile.transform->position.y - player_collider.transform->position.y) *
					(current_projectile.transform->position.y - player_collider.transform->position.y)) +

					((current_projectile.transform->position.z - player_collider.transform->position.z) * 
					(current_projectile.transform->position.z - player_collider.transform->position.z))
				);

			if (distance <= current_projectile.collider.radius + player_collider.radius){
					handle_player_collision();
				}
		}

	}

	// update projectile's life
	{
		current_projectile.lifetime -= elapsed;
		if (current_projectile.lifetime <= 0.0f){
			destroy_projectile();
		}
	}
	
	//move camera:
	{

		//combine inputs into a move:
		constexpr float PlayerSpeed = 0.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}


	// check game state
	{
		if (targets_left <= 0)
		{
			game_state_message = "YOU WIN!";
			current_game_state = GameState::PostRound;
		}

		if (health_points <= 0)
		{
			game_state_message = "YOU LOSE!";
			current_game_state = GameState::PostRound;
		}
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

	GL_ERRORS(); //print any errors produced by this setup code

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
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse motion rotates camera;escape ungrabs mouse",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		
		// Health Points
		lines.draw_text("Health Points ",
			glm::vec3(-aspect + 0.1f * H + ofs, 0.85f, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		lines.draw_text(std::to_string(health_points),
			glm::vec3(-aspect + 0.1f * H + ofs + 0.5, 0.85f, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		
		// Game State Message
		lines.draw_text(game_state_message,
			glm::vec3(-0.15f, 0.0f, 0.0f),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}

// based on 15-466 Lecture 4 Add to Scene Graph Example
void PlayMode::spawn_projectile(glm::vec3 spawn_position, glm::vec3 direction, float speed)
{
	scene.transforms.emplace_back();
	current_projectile = Projectile(&scene.transforms.back());

	current_projectile.transform->position = spawn_position;
	current_projectile.velocity = speed * direction;
	current_projectile.active = true;


	Mesh const &projectile_mesh = game_meshes->lookup("Sphere");
	scene.drawables.emplace_back(current_projectile.transform);
	Scene::Drawable &drawable = scene.drawables.back();

	drawable.pipeline = lit_color_texture_program_pipeline;
	drawable.pipeline.vao = game_meshes_for_lit_color_texture_program;
	drawable.pipeline.type = projectile_mesh.type;
	drawable.pipeline.start = projectile_mesh.start;
	drawable.pipeline.count = projectile_mesh.count;

	current_game_state = GameState::MidRound;
}

// based on 15-466 Lecture 4 Remove from Scene Graph Example
void PlayMode::destroy_projectile()
{
	current_projectile.active = false;

	// clean up drawable
	for (std::list<Scene::Drawable>::iterator di = scene.drawables.begin(); di != scene.drawables.end();){
		if (di->transform == current_projectile.transform){
			di = scene.drawables.erase(di);
		} else {
			di++;
		}
	}

	// player missed
	if (current_game_state == GameState::MidRound){
		shoot_player();
	}
}

void PlayMode::handle_target_collision(Target& target)
{
	// Update Game State
	current_game_state = GameState::Play;
	targets_left--;
	
	// Remove Projectile
	current_projectile.active = false;
	destroy_projectile();

	// Deactivate Target
	target.transform->rotation = glm::angleAxis(glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	target.active = false;
}

void PlayMode::handle_player_collision()
{
	// Update GameState
	current_game_state = GameState::Play;
	health_points--;
	
	// Remove Projectile
	current_projectile.active = false;
	destroy_projectile();

	// Reset Cannon Rotation
	cannon->rotation = cannon_base_rotation;
}

void PlayMode::shoot_player()
{
	cannon->rotation = cannon_target_rotation;

	glm::mat4x3 cannon_matrix = cannon->make_parent_from_local();
	glm::vec3 direction = cannon_matrix[2];

	spawn_projectile(cannon->position, direction, cannon_projectile_speed);
}