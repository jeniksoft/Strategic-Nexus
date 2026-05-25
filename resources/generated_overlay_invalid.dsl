campaign "campaign_001" {
  empire "empire_001" {
    rule "bad_fleet_micro" {
      ministry = military_ministry
      prefer fleet_target enemy_capital intensity 1.0
      duration = next_session
      confidence = 0.9
      rationale = "Invalid tactical control request."
    }
  }
}
