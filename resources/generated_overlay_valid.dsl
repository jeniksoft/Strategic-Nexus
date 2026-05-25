campaign "campaign_001" {
  empire "empire_001" {
    rule "border_war_defense" {
      ministry = military_ministry
      when personality.paranoia >= medium
      when memory.repeated_border_war
      prefer military_posture defensive intensity 0.7
      prefer doctrine_inertia high intensity 0.4
      duration = next_session
      confidence = 0.72
      rationale = "Repeated border wars reinforced defensive paranoia."
    }
  }
}
