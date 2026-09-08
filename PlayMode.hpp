#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

enum GameState {
	Play,
	MidRound,
	PostRound
};

struct SphereCollider {
	Scene::Transform* transform = nullptr;
	float radius = 0.0f;

	SphereCollider(Scene::Transform *transform, float radius) :
					transform(transform), radius(radius) {}

	~SphereCollider() {}
};

struct Projectile {
	Scene::Transform* transform = nullptr;
	glm::vec3 velocity = glm::vec3(0.0f);
	float lifetime = 2.0f;
	bool active = false;

	SphereCollider collider = SphereCollider(transform, 1.0f); // default radius

	Projectile(Scene::Transform *transform) :
				transform(transform) {}

	~Projectile() {}
};

struct Target {
	Scene::Transform* transform = nullptr;
	SphereCollider collider = SphereCollider(transform, 1.0f); // default radius
	bool active = true;

	Target(Scene::Transform *transform) :
			transform(transform) {}

	~Target() {}
};

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
	} left, right, down, up, enter;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	//camera:
	Scene::Camera *camera = nullptr;
	float wobble = 0.0f;

	/*----------------------- GAME LOOP ----------------------*/
	
	GameState current_game_state = GameState::Play;
	std::string game_state_message = "";

	/*----------------------- PLAYER ------------------------*/
	int health_points = 0;
	SphereCollider player_collider = SphereCollider(nullptr, 1.0f);
	float player_projectile_speed = 50.0f;

	
	/*----------------------- PROJECTILE ------------------------*/
	Projectile current_projectile = Projectile(nullptr);
	float gravity = -20.0f;

	void spawn_projectile(glm::vec3 spawn_position, glm::vec3 direction, float speed);
	void destroy_projectile();

	void handle_target_collision(Target& target);
	void handle_player_collision();
	
	/*----------------------- ENEMY -------------------------*/
	int targets_left = 0;
	int total_targets = 0;
	std::vector<Target> targets;
	
	// Cannon
	Scene::Transform* cannon = nullptr;
	glm::quat cannon_base_rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::quat cannon_target_rotation = glm::angleAxis(glm::radians(100.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	float cannon_projectile_speed = 70.0f;
	void shoot_player();
};
