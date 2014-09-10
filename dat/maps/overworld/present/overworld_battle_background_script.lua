local ns = {}
setmetatable(ns, {__index = _G})
overworld_battle_background_script = ns;
setfenv(1, ns);

-- Set the correct battle background according to the zone type
function Initialize(battle_instance)

    local background_type = GlobalManager:GetEventValue("overworld", "battle_background");
    if (background_type == 0) then return end

    local background_file = "";

    if (background_type == 2) then
        background_file = "img/backdrops/battle/forest_background.png";
        --battle_instance:GetScriptSupervisor():AddScript("dat/battles/forest_battle_anim.lua");
    elseif (background_type == 3) then
        background_file = "img/backdrops/battle/desert_background.png";
        --battle_instance:GetScriptSupervisor():AddScript("dat/battles/desert_battle_anim.lua");
    elseif (background_type == 4) then
        background_file = "img/backdrops/battle/snow_background.png";
        --battle_instance:GetScriptSupervisor():AddScript("dat/battles/snow_battle_anim.lua");
    else -- == 1 or else
        background_file = "img/backdrops/battle/plains_background.png";
    end

    battle_instance:GetMedia():SetBackgroundImage(background_file);
end
