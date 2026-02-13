use matrix_sdk::Client;
use matrix_sdk::ruma::{RoomId, UserId, RoomAliasId, OwnedRoomOrAliasId};

#[derive(Debug, PartialEq)]
pub enum CommandResult {
    /// The command was handled internally (e.g. joined a room), stop processing.
    Handled,
    /// The command modified the text (e.g. /shrug), continue sending the returned string.
    Continue(String),
}

pub async fn handle_slash_command(client: &Client, room_id: &RoomId, text: &str) -> Result<CommandResult, anyhow::Error> {
    if !text.starts_with('/') {
        return Ok(CommandResult::Continue(text.to_string()));
    }

    // Handle escaping: "//" -> "/"
    if text.starts_with("//") {
        return Ok(CommandResult::Continue(text[1..].to_string()));
    }

    let (cmd, args) = match text[1..].split_once(' ') {
        Some((c, a)) => (c, a.trim()),
        None => (text[1..].trim(), ""),
    };

    match cmd {
        "shrug" => {
            let new_text = if args.is_empty() {
                "¯\\_(ツ)_/¯".to_string()
            } else {
                format!("{} ¯\\_(ツ)_/¯", args)
            };
            Ok(CommandResult::Continue(new_text))
        },
        "join" => {
            log::info!("Handling /join command for: {}", args);
            // Try parsing as RoomID or Alias
            if let Ok(rid) = <&RoomId>::try_from(args) {
                 client.join_room_by_id(rid).await?;
            } else if let Ok(alias) = <&RoomAliasId>::try_from(args) {
                 client.join_room_by_id_or_alias(alias.into(), &[]).await?;
            } else {
                // Try treating as alias anyway? Or return error?
                // For now, let's try assuming it's an alias if it starts with #
                 if args.starts_with('#') {
                      let alias = RoomAliasId::parse(args)?;
                      let room_or_alias: OwnedRoomOrAliasId = alias.into();
                      client.join_room_by_id_or_alias(&room_or_alias, &[]).await?;
                 } else {
                      return Err(anyhow::anyhow!("Invalid room ID or alias"));
                 }
            }
            Ok(CommandResult::Handled)
        },
        "invite" => {
            log::info!("Handling /invite command for: {}", args);
            if let Some(room) = client.get_room(room_id) {
                if let Ok(user_id) = <&UserId>::try_from(args) {
                     room.invite_user_by_id(user_id).await?;
                } else {
                     return Err(anyhow::anyhow!("Invalid User ID"));
                }
            }
            Ok(CommandResult::Handled)
        },
        "poll" => {
            log::info!("Handling /poll command: {}", args);
            let parts = parse_quoted_args(args);
            if parts.len() < 3 {
                 return Err(anyhow::anyhow!("Usage: /poll \"Question\" \"Option1\" \"Option2\"..."));
            }
            
            if let Some(room) = client.get_room(room_id) {
                 use matrix_sdk::ruma::events::poll::start::{PollStartEventContent, PollAnswer, PollContentBlock, PollAnswers};
                 // Try finding TextContentBlock in events::message or events::room::message
                 // If it fails, we might need a different path. 
                 // Based on search, it might be in `matrix_sdk::ruma::events::message`.
                 use matrix_sdk::ruma::events::message::TextContentBlock; 
                
                 let question_str = parts[0].clone();
                 let answers_str = &parts[1..];
                 
                 // Create answers
                 let poll_answers_vec: Vec<PollAnswer> = answers_str.iter().enumerate().map(|(i, a)| {
                     PollAnswer::new(i.to_string(), TextContentBlock::plain(a))
                 }).collect();
                 
                 // PollAnswers might be a struct wrapping Vec. Use TryFrom or From if available.
                 // If it expects PollAnswers, and we have Vec<PollAnswer>, try .into()
                 let poll_answers: PollAnswers = poll_answers_vec.try_into().map_err(|_| anyhow::anyhow!("Failed to create poll answers"))?; // Assuming TryFrom<Vec>
                 
                 // Create PollContentBlock (Question + Answers)
                 let poll_content = PollContentBlock::new(
                     TextContentBlock::plain(&question_str),
                     poll_answers
                 );
                 
                 // Create fallback text
                 let fallback_text = format!("Poll: {}\nOptions: {}", question_str, answers_str.join(", "));
                 
                 // Create Event Content
                 let content = PollStartEventContent::new(
                     TextContentBlock::plain(fallback_text),
                     poll_content
                 );
                 
                 room.send(content).await?;
            }
            Ok(CommandResult::Handled)
        },
        _ => {
            // Unknown command, treat as explicit text
            Ok(CommandResult::Continue(text.to_string()))
        }
    }
}

fn parse_quoted_args(input: &str) -> Vec<String> {
    let mut args = Vec::new();
    let mut current = String::new();
    let mut in_quote = false;
    let mut escaped = false;

    for c in input.chars() {
        if escaped {
            current.push(c);
            escaped = false;
        } else if c == '\\' {
            escaped = true;
        } else if c == '"' {
            in_quote = !in_quote;
        } else if c == ' ' && !in_quote {
            if !current.is_empty() {
                args.push(current.clone());
                current.clear();
            }
        } else {
            current.push(c);
        }
    }
    if !current.is_empty() {
        args.push(current);
    }
    args
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_quoted_args() {
        // Basic quoted
        let input = "\"Question\" \"Option 1\" \"Option 2\"";
        let args = parse_quoted_args(input);
        assert_eq!(args.len(), 3);
        assert_eq!(args[0], "Question");
        assert_eq!(args[1], "Option 1");
        assert_eq!(args[2], "Option 2");
        
        // Mixed quotes and plain
        let input2 = "Question \"Option 1\" Option2";
        let args2 = parse_quoted_args(input2);
        assert_eq!(args2.len(), 3);
        assert_eq!(args2[0], "Question");
        assert_eq!(args2[1], "Option 1");
        assert_eq!(args2[2], "Option2");

        // Escaped quotes
        let input3 = "\"Said \\\"Hello\\\"\"";
        let args3 = parse_quoted_args(input3);
        assert_eq!(args3.len(), 1);
        assert_eq!(args3[0], "Said \"Hello\"");
        
        // Multiple spaces
        let input4 = "One   Two";
        let args4 = parse_quoted_args(input4);
        assert_eq!(args4.len(), 2);
        assert_eq!(args4[0], "One");
        assert_eq!(args4[1], "Two");
    }
}

