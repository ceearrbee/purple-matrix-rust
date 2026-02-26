use matrix_sdk::deserialized_responses::TimelineEvent;
fn inspect(e: TimelineEvent) {
    let _ = e.kind;
}
